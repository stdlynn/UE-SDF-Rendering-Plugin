// Plugin
#include "FBrickmapBuilder.h"

// UE
#include "EngineUtils.h"

void FBrickmapBuilder::AllocateResources()
{
	check(IsInRenderingThread());

	const FRDGTextureDesc BrickAtlasDesc = FRDGTextureDesc::Create3D(
		AtlasSize, PF_R8, FClearValueBinding::None,
		TexCreate_ShaderResource | TexCreate_UAV);
	RT.BrickAtlas = AllocatePooledTexture(BrickAtlasDesc, TEXT("SDF_BrickAtlas"));

	const uint32 GridSize = PtrGridSize.X * PtrGridSize.Y * PtrGridSize.Z;
	const FRDGBufferDesc PtrGridDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), GridSize);
	RT.PtrGrid = AllocatePooledBuffer(PtrGridDesc, TEXT("SDF_BrickPtrBuffer"));

	constexpr uint32 numCounters = 1;
	const FRDGBufferDesc CounterDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), numCounters);
	RT.BrickAllocCounter = AllocatePooledBuffer(CounterDesc, TEXT("SDF_BrickAllocCounter"));

	UE_LOG(LogTemp, Log, TEXT("SDFPlugin: Allocated Brickmap"));
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Allocated Brickmap"));
}

void FBrickmapBuilder::ReleaseResources()
{
	check(IsInRenderingThread());
	RT.BrickAtlas.SafeRelease();
	RT.PtrGrid.SafeRelease();
	RT.BrickAllocCounter.SafeRelease();
}

bool FBrickmapBuilder::AreResourcesValid() const
{
	return RT.BrickAtlas.IsValid() 
		&& RT.PtrGrid.IsValid()
		&& RT.BrickAllocCounter.IsValid();
}

