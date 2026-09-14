// Plugin
#include "FBrickmapBuilder.h"
#include "BrickmapCommon.h"

// UE
#include "EngineUtils.h"

struct FAABB
{
	FVector3f Min;
	FVector3f Max;
};

FBrickmapBuilder::FBrickmapBuilder()
{
	RT.BrickListCPU.SetNum((PtrGridSize.X * PtrGridSize.Y * PtrGridSize.Z) / 32);
}

bool FBrickmapBuilder::GetBrickListBit(uint32 Idx) const
{
	const uint32 ArrayIdx = Idx / 32;
	const uint32 BitIdx = Idx % 32;
	return (RT.BrickListCPU[ArrayIdx] & (1u << BitIdx)) != 0;
}

void FBrickmapBuilder::SetBrickListBit(uint32 Idx, bool bValue)
{
	const uint32 ArrayIdx = Idx / 32;
	const uint32 BitIdx = Idx % 32;
	if (bValue)
	{
		RT.BrickListCPU[ArrayIdx] |= (1u << BitIdx);
	}
	else
	{
		RT.BrickListCPU[ArrayIdx] &= ~(1u << BitIdx);
	}
}

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

	constexpr uint32 numCounters = 1;
	const FRDGBufferDesc CounterDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), numCounters);
	RT.BrickAllocCounter = AllocatePooledBuffer(CounterDesc, TEXT("SDF_BrickAllocCounter"));

	constexpr uint32 maxUpdatesPerFrame = 1024;
	const FRDGBufferDesc BrickUpdateDesc = FRDGBufferDesc::CreateStructuredDesc(
		sizeof(FGPUSDF), maxUpdatesPerFrame);
	RT.BrickUpdateList = AllocatePooledBuffer(BrickUpdateDesc, TEXT("SDF_BrickUpdateList"));

	UE_LOG(LogTemp, Log, TEXT("SDFPlugin: Allocated Brickmap"));
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Allocated Brickmap"));
}

void FBrickmapBuilder::ReleaseResources()
{
	check(IsInRenderingThread());
	RT.BrickAtlas.SafeRelease();
	RT.PtrGrid.SafeRelease();
	RT.BrickAllocCounter.SafeRelease();
	RT.BrickUpdateList.SafeRelease();
}

bool FBrickmapBuilder::AreResourcesValid() const
{
	return RT.BrickAtlas.IsValid() 
		&& RT.PtrGrid.IsValid()
		&& RT.BrickAllocCounter.IsValid()
		&& RT.BrickUpdateList.IsValid();
}

FRDGPooledBuffer* FBrickmapBuilder::GetBrickUpdateList(const TArray<FGPUSDF>& SDFs)
{
	RT.BrickListCPU.Empty((PtrGridSize.X * PtrGridSize.Y * PtrGridSize.Z) / 32);

	for (auto Edit : SDFs)
	{
		TArray<uint64> BrickList;
		
		// Find bricks within edit's AABB
		FAABB AABB;
		AABB.Min = Edit.SDF.Pos - Edit.SDF.Radius;
		AABB.Max = Edit.SDF.Pos + Edit.SDF.Radius;

		FUintVector MinCell = WorldPosToGridCell(AABB.Min);
		FUintVector MaxCell = WorldPosToGridCell(AABB.Max);
		uint32 Width = MaxCell.X - MinCell.X + 1;
		uint32 Height = MaxCell.Y - MinCell.Y + 1;
		uint32 Depth = MaxCell.Z - MinCell.Z + 1;

		for (uint32 z = 0; z < Depth; z++)
		{
			for (uint32 y = 0; y < Height; y++)
			{
				for (uint32 x = 0; x < Width; x++)
				{
					FUintVector GridCell = MinCell + FUintVector(x, y, z);
					uint32 BrickIndex = GridCellToGridIdx(GridCell);
					BrickList.Add(BrickIndex);
				}
			}
		}

		// Cull non-touching bricks
		for (int i = 0; i < BrickList.Num(); i++)
		{
			uint32 BrickIndex = BrickList[i];
			FUintVector3 GridCell = GridIdxToGridCell(BrickIndex);
			FVector3f World = GridCellToWorldPos(GridCell) + FVector3f(VoxelResolution / 2.0f);

			FVector3f Diff = Edit.SDF.Pos - World;
			FVector3f Delta = Diff / FMath::Abs(Diff); // either -1 or +1 per axis
			FVector3f ClosestCorner = World + Delta * FVector3f(VoxelResolution / 2.0f);
			FVector3f FarthestCorner = World - Delta * FVector3f(VoxelResolution / 2.0f);

			float ClosestDist = SDFSphere(ClosestCorner, Edit.SDF.Pos, Edit.SDF.Radius);
			float FarthestDist = SDFSphere(FarthestCorner, Edit.SDF.Pos, Edit.SDF.Radius);
			bool bInsideClose = ClosestDist < Edit.SDF.Radius;
			bool bInsideFar = FarthestDist < Edit.SDF.Radius;

			// Check if the brick touches the shell
			if (bInsideClose && not bInsideFar)
			{
				SetBrickListBit(BrickIndex, true);
			}
		}
	}

	// Copy brick list to the GPU buffer


	return RT.BrickUpdateList;
}