// Plugin
#include "FBrickmapBuilder.h"
#include "BrickmapCommon.h"

void FBrickmapBuilder::AllocateResources()
{
	check(IsInRenderingThread());

	const FRDGTextureDesc BrickAtlasDesc = FRDGTextureDesc::Create3D(
		FIntVector(AtlasSize), PF_R8, FClearValueBinding::None,
		TexCreate_ShaderResource | TexCreate_UAV);
	RT.BrickAtlas = AllocatePooledTexture(BrickAtlasDesc, TEXT("SDF_BrickAtlas"));

	const uint32 GridSize = PtrGridSize.X * PtrGridSize.Y * PtrGridSize.Z;
	const FRDGBufferDesc PtrGridDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), GridSize);
	RT.PtrGrid = AllocatePooledBuffer(PtrGridDesc, TEXT("SDF_BrickPtrBuffer"));

	constexpr uint32 NumCounters = 4;
	const FRDGBufferDesc CounterDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), NumCounters);
	RT.BrickAllocCounters = AllocatePooledBuffer(CounterDesc, TEXT("SDF_BrickAllocCounter"));

	constexpr uint32 DeallocatorCapacity = NumAtlasBricks.X * NumAtlasBricks.Y * NumAtlasBricks.Z;
	const FRDGBufferDesc FreedListDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), DeallocatorCapacity);
	const FRDGBufferDesc DeallocatedListDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), DeallocatorCapacity);
	RT.FreedListResource = AllocatePooledBuffer(FreedListDesc,		 TEXT("SDF_BrickFreedList"));
	RT.FreedListResource = AllocatePooledBuffer(DeallocatedListDesc, TEXT("SDF_BrickDeallocatedList"));

	UE_LOG(LogTemp, Log, TEXT("SDFPlugin: Allocated Brickmap"));
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Allocated Brickmap"));
}

void FBrickmapBuilder::ReleaseResources()
{
	check(IsInRenderingThread());
	RT.BrickAtlas.SafeRelease();
	RT.PtrGrid.SafeRelease();
	RT.BrickAllocCounters.SafeRelease();
	RT.UpdateListResource.SafeRelease();
}

bool FBrickmapBuilder::AreResourcesValid() const
{
	return RT.BrickAtlas.IsValid() 
		&& RT.PtrGrid.IsValid()
		&& RT.BrickAllocCounters.IsValid()
		&& RT.UpdateListResource.IsValid();
}

void FBrickmapBuilder::SetBrickUpdateList(const TArray<FGPUSDF>& SDFs)
{
	RT.UpdateListCPU.Empty();

	const FIntVector3 GridMax = FIntVector3(PtrGridSize.X, PtrGridSize.Y, PtrGridSize.Z) - FIntVector3(1);

	for (auto [SDF, Idx] : SDFs)
	{
		// TODO temporary hack to test deallocation (put const ref back when removing this)
		// Grow the sphere so it also encapsulates its previous position
		constexpr float RadiusOffset = VoxelResolution;
		SDF.Radius += RadiusOffset;
		FIntVector3 MinCell = WorldPosToGridCellSigned(SDF.Pos - SDF.Radius);
		FIntVector3 MaxCell = WorldPosToGridCellSigned(SDF.Pos + SDF.Radius);

		MinCell = FIntVector3(
			FMath::Clamp(MinCell.X, 0, GridMax.X),
			FMath::Clamp(MinCell.Y, 0, GridMax.Y),
			FMath::Clamp(MinCell.Z, 0, GridMax.Z));
		MaxCell = FIntVector3(
			FMath::Clamp(MaxCell.X, 0, GridMax.X),
			FMath::Clamp(MaxCell.Y, 0, GridMax.Y),
			FMath::Clamp(MaxCell.Z, 0, GridMax.Z));

		// @formatter:off
		for (int32 z = MinCell.Z; z <= MaxCell.Z; z++) {
		for (int32 y = MinCell.Y; y <= MaxCell.Y; y++) {
		for (int32 x = MinCell.X; x <= MaxCell.X; x++)
		{
			const FUintVector3 GridCell(
				static_cast<uint32>(x), static_cast<uint32>(y), static_cast<uint32>(z));

			const FVector3f Min = GridCellToWorldPos(GridCell);
			const FVector3f Max = Min + FVector3f(VoxelResolution);

			const FVector3f ClosestPoint(
				FMath::Clamp(SDF.Pos.X, Min.X, Max.X),
				FMath::Clamp(SDF.Pos.Y, Min.Y, Max.Y),
				FMath::Clamp(SDF.Pos.Z, Min.Z, Max.Z));

			const FVector3f FarthestPoint(
				FMath::Abs(SDF.Pos.X - Min.X) > FMath::Abs(SDF.Pos.X - Max.X) ? Min.X : Max.X,
				FMath::Abs(SDF.Pos.Y - Min.Y) > FMath::Abs(SDF.Pos.Y - Max.Y) ? Min.Y : Max.Y,
				FMath::Abs(SDF.Pos.Z - Min.Z) > FMath::Abs(SDF.Pos.Z - Max.Z) ? Min.Z : Max.Z);


			if (SDFSphere(ClosestPoint, SDF.Pos, SDF.Radius) <= 0.0f
				&& SDFSphere(FarthestPoint, SDF.Pos, SDF.Radius) >= 0.0f)
			{
				RT.UpdateListCPU.Add(GridCellToGridIdx(GridCell));
			}
		}}}
		// @formatter:on
	}
}

void FBrickmapBuilder::AllocateBrickList()
{
	// TODO look into if this should be a per-frame allocated buffer
	if (not RT.UpdateListResource.IsValid() || RT.UpdateListCPU.Num() > RT.UpdateListResource->GetSize() / sizeof(uint32))
	{
		const uint32 Capacity = FMath::Max(RT.UpdateListCPU.Num(), 1);

		FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateUploadDesc(sizeof(uint32), Capacity);
		RT.UpdateListResource = AllocatePooledBuffer(BufferDesc, TEXT("Brick List Update Buffer"));
	}
}

FRDGBufferRef FBrickmapBuilder::RegisterBrickListBuffer(FRDGBuilder& GraphBuilder) const
{
	TArray<uint32> BrickArray = RT.UpdateListCPU.Array();
	
	const FRDGBufferRef BrickListRDG{ GraphBuilder.RegisterExternalBuffer(RT.UpdateListResource, 
																		  ERDGBufferFlags::MultiFrame) };

	GraphBuilder.QueueBufferUpload(BrickListRDG, BrickArray.GetData(), 
								   BrickArray.Num() * sizeof(uint32), ERDGInitialDataFlags::None);

	return BrickListRDG;
}
