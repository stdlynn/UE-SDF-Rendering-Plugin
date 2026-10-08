// Plugin
#include "FBrickmapBuilder.h"
#include "BrickmapCommon.h"
#include "SDFEdits.h"
#include "SDFFunctions.h"

float FBrickmapBuilder::GetTerrainHeightMin( const FTerrainParams& TerrainParams )
{
	return TerrainParams.PlanePos.Z;
}

float FBrickmapBuilder::GetTerrainHeightMax( const FTerrainParams& TerrainParams )
{
	const FTerrainParams& P = TerrainParams;
	const float Amplitude0 = P.Amplitude / P.Frequency * (P.bSimplex ? sqrt(3.0f) / 2.0f : 1.0f);
	const float Inflation0 = P.Inflation * (P.VerticalNoiseOffset + 0.5f) * Amplitude0;
	const float Smoothing0 = P.UnionSmoothing * Amplitude0;
	const float Gain = P.InflationGain * P.Persistence;
	const float GainN = (Gain == 1.0f) ? P.NumOctaves : (1.0f - pow(Gain, P.NumOctaves)) / (1.0f - Gain);
	const float InflationN = Inflation0 * GainN;
	const float PersistenceN = (P.Persistence == 1.0f) ? P.NumOctaves : (1.0f - pow(P.Persistence, P.NumOctaves)) / (1.0f - P.Persistence);
	const float SmoothingN = Smoothing0 * PersistenceN;

	const float MaxHeight = P.PlanePos.Z + InflationN + SmoothingN;
	return MaxHeight;
}

void FBrickmapBuilder::AllocateResources()
{
	check( IsInRenderingThread() );

	const FRDGTextureDesc BrickAtlasDesc = FRDGTextureDesc::Create3D( FIntVector( AtlasSize ), PF_R16F,
																	  FClearValueBinding::None,
																	  TexCreate_ShaderResource | TexCreate_UAV );
	RT.BrickAtlas = AllocatePooledTexture( BrickAtlasDesc, TEXT( "SDF_BrickAtlas" ) );

	const uint32 GridSize = PtrGridSize.X * PtrGridSize.Y * PtrGridSize.Z;
	const FRDGBufferDesc PtrGridDesc = FRDGBufferDesc::CreateBufferDesc( sizeof( uint32 ), GridSize );
	RT.PtrGrid = AllocatePooledBuffer( PtrGridDesc, TEXT( "SDF_BrickPtrBuffer" ) );

	constexpr uint32 NumCounters = 4;
	const FRDGBufferDesc CounterDesc = FRDGBufferDesc::CreateBufferDesc( sizeof( uint32 ), NumCounters );
	RT.BrickAllocCounters = AllocatePooledBuffer( CounterDesc, TEXT( "SDF_BrickAllocCounter" ) );

	const uint32 DeallocatorCapacity = NumAtlasBricks.X * NumAtlasBricks.Y * NumAtlasBricks.Z;
	const FRDGBufferDesc FreedListDesc = FRDGBufferDesc::CreateBufferDesc( sizeof( uint32 ), DeallocatorCapacity );
	const FRDGBufferDesc DeallocatedListDesc = FRDGBufferDesc::CreateBufferDesc( sizeof( uint32 ), DeallocatorCapacity );
	RT.FreedListResource = AllocatePooledBuffer( FreedListDesc, TEXT( "SDF_BrickFreedList" ) );
	RT.DeallocatedListResource = AllocatePooledBuffer( DeallocatedListDesc, TEXT( "SDF_BrickDeallocatedList" ) );

	UE_LOG( LogTemp, Log, TEXT("SDFPlugin: Allocated Brickmap") );
	GEngine->AddOnScreenDebugMessage( -1, 2.0f, FColor::Green, TEXT( "Allocated Brickmap" ) );
}

void FBrickmapBuilder::ReleaseResources()
{
	check( IsInRenderingThread() );
	RT.BrickAtlas.SafeRelease();
	RT.PtrGrid.SafeRelease();
	RT.BrickAllocCounters.SafeRelease();
	RT.UpdateListResource.SafeRelease();
	RT.FreedListResource.SafeRelease();
	RT.DeallocatedListResource.SafeRelease();
}

bool FBrickmapBuilder::AreResourcesValid() const
{
	return RT.BrickAtlas.IsValid()
		   && RT.PtrGrid.IsValid()
		   && RT.BrickAllocCounters.IsValid()
		   && RT.UpdateListResource.IsValid()
		   && RT.FreedListResource.IsValid()
		   && RT.DeallocatedListResource.IsValid();
}

void FBrickmapBuilder::ClearBrickmap(FRDGBuilder& GraphBuilder )
{
	const FRDGTextureRef BrickAtlasRDG = GraphBuilder.RegisterExternalTexture(GetBrickAtlas(), ERDGTextureFlags::MultiFrame);
	const FRDGBufferRef PtrBufferRDG = GraphBuilder.RegisterExternalBuffer(GetPtrGrid(), ERDGBufferFlags::MultiFrame);
	const FRDGBufferRef AllocCountersRDG = GraphBuilder.RegisterExternalBuffer(GetBrickAllocCounter(), ERDGBufferFlags::MultiFrame);
	const FRDGBufferRef FreedListRDG = GraphBuilder.RegisterExternalBuffer(GetFreedList(), ERDGBufferFlags::MultiFrame);
	const FRDGBufferRef DeallocatedListRDG = GraphBuilder.RegisterExternalBuffer(GetDeallocatedList(), ERDGBufferFlags::MultiFrame);

	const FRDGBufferUAVRef PtrGridUAV = GraphBuilder.CreateUAV(PtrBufferRDG, PF_R32_UINT);
	const FRDGTextureUAVRef BrickAtlasUAV = GraphBuilder.CreateUAV(BrickAtlasRDG);
	const FRDGBufferUAVRef FreedListUAV = GraphBuilder.CreateUAV(FreedListRDG, PF_R32_UINT);
	const FRDGBufferUAVRef DeallocatedListUAV = GraphBuilder.CreateUAV(DeallocatedListRDG, PF_R32_UINT);
	const FRDGBufferUAVRef AllocCountersUAV = GraphBuilder.CreateUAV(AllocCountersRDG, PF_R32_UINT);

	AddClearUAVPass(GraphBuilder, PtrGridUAV, 0xFFFFFFFFu);
	AddClearUAVPass(GraphBuilder, BrickAtlasUAV, 1.0f);
	AddClearUAVPass(GraphBuilder, FreedListUAV, 0xFFFFFFFFu);
	AddClearUAVPass(GraphBuilder, DeallocatedListUAV, 0xFFFFFFFFu);
	AddClearUAVPass(GraphBuilder, AllocCountersUAV, 0u);
	bReset = false;
	bTerrainDirty = true;
}

void FBrickmapBuilder::SetBrickUpdateList( const TArray<FGPUSDF>& SDFs, const FVector3f& PtrGridPos, float VoxSize )
{
	RT.UpdateListCPU.Empty();

	const float BrickSize{ GetBrickSize( VoxSize ) };
	const FVector3f GridMin{ GetGridMin( PtrGridPos, BrickSize ) };
	const FIntVector3 GridMax = FIntVector3( PtrGridSize.X, PtrGridSize.Y, PtrGridSize.Z );

	// TODO temporary hack to test deallocation
	// Add some thickness so it also encapsulates its previous position
	const float Thickness = BrickSize * 4;

	for ( const auto& SDF : SDFs )
	{
		FAABB AABB = AABB::GetSDFAABB( SDF );
		const FVector3f MinF = ( AABB.Min - GridMin - Thickness ) / BrickSize;
		const FVector3f MaxF = ( AABB.Max - GridMin + Thickness ) / BrickSize;

		const FIntVector3 MinCell(FMath::Clamp( FMath::FloorToInt32( MinF.X ), 0, GridMax.X - 1 ),
								  FMath::Clamp( FMath::FloorToInt32( MinF.Y ), 0, GridMax.Y - 1 ),
								  FMath::Clamp( FMath::FloorToInt32( MinF.Z ), 0, GridMax.Z - 1 ));
		const FIntVector3 MaxCell(FMath::Clamp( FMath::FloorToInt32( MaxF.X ), 0, GridMax.X - 1 ),
								  FMath::Clamp( FMath::FloorToInt32( MaxF.Y ), 0, GridMax.Y - 1 ),
								  FMath::Clamp( FMath::FloorToInt32( MaxF.Z ), 0, GridMax.Z - 1 ));

		// @formatter:off
		for ( int32 z = MinCell.Z; z <= MaxCell.Z; z++ )
		for ( int32 y = MinCell.Y; y <= MaxCell.Y; y++ )
		for ( int32 x = MinCell.X; x <= MaxCell.X; x++ )
		{
			const FIntVector3 GridCell( x, y, z );

			const FVector3f Min = GridCellToWorldPos( GridCell, GridMin, BrickSize );
			const FVector3f Max = Min + BrickSize;

			const FVector3f ClosestPoint( FMath::Clamp( SDF.Pos.X, Min.X, Max.X ),
										  FMath::Clamp( SDF.Pos.Y, Min.Y, Max.Y ),
										  FMath::Clamp( SDF.Pos.Z, Min.Z, Max.Z ) );

			const FVector3f FarthestPoint( FMath::Abs( SDF.Pos.X - Min.X ) > FMath::Abs( SDF.Pos.X - Max.X ) ? Min.X : Max.X,
										   FMath::Abs( SDF.Pos.Y - Min.Y ) > FMath::Abs( SDF.Pos.Y - Max.Y ) ? Min.Y : Max.Y,
										   FMath::Abs( SDF.Pos.Z - Min.Z ) > FMath::Abs( SDF.Pos.Z - Max.Z ) ? Min.Z : Max.Z );

			const float ClosestDist = DistFn::DistOrigin(SDF, ClosestPoint - SDF.Pos);
			const float FarthestDist = DistFn::DistOrigin(SDF, FarthestPoint - SDF.Pos);
			if (ClosestDist <= Thickness && FarthestDist >= -Thickness)
			{
				RT.UpdateListCPU.Add( GridCellToGridIdx( FUintVector3{ GridCell } ) );
			}
		}
		// @formatter:on
	}
}

void FBrickmapBuilder::AllocateBrickList()
{
	// TODO look into if this should be a per-frame allocated buffer
	if (not RT.UpdateListResource.IsValid() || RT.UpdateListCPU.Num() > RT.UpdateListResource->GetSize() / sizeof( uint32 ))
	{
		const uint32 Capacity = FMath::Max( RT.UpdateListCPU.Num(), 1 );

		FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateUploadDesc( sizeof( uint32 ), Capacity );
		RT.UpdateListResource = AllocatePooledBuffer( BufferDesc, TEXT( "Brick List Update Buffer" ) );
	}
}

FRDGBufferRef FBrickmapBuilder::RegisterBrickListBuffer( FRDGBuilder& GraphBuilder ) const
{
	TArray<uint32> BrickArray = RT.UpdateListCPU.Array();

	const FRDGBufferRef BrickListRDG{ GraphBuilder.RegisterExternalBuffer( RT.UpdateListResource, ERDGBufferFlags::MultiFrame ) };

	GraphBuilder.QueueBufferUpload( BrickListRDG,
									BrickArray.GetData(),
									BrickArray.Num() * sizeof( uint32 ),
									ERDGInitialDataFlags::None );

	return BrickListRDG;
}