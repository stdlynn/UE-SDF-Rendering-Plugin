#pragma once

// Plugin
#include "SDFEdits.h"

// UE
#include "CoreMinimal.h"
#include "PixelShaderUtils.h"
#include "DataDrivenShaderPlatformInfo.h"

class FBrickmapBuilder
{
public:

	FBrickmapBuilder();

	[[nodiscard]] IPooledRenderTarget* GetBrickAtlas() const { return RT.BrickAtlas; }
	[[nodiscard]] FRDGPooledBuffer* GetPtrGrid() const { return RT.PtrGrid; }
	[[nodiscard]] FRDGPooledBuffer* GetBrickAllocCounter() const { return RT.BrickAllocCounter; }
	[[nodiscard]] uint32 GetLastBuildFrame() const { return RT.LastBuildFrame; }
	void SetLastBuildFrame(const uint32 Frame) { RT.LastBuildFrame = Frame; }

	void AllocateResources();
	void ReleaseResources();
	bool AreResourcesValid() const;

	FRDGPooledBuffer* GetBrickUpdateList(const TArray<FGPUSDF>& SDFs);

private:

	[[nodiscard]] bool GetBrickListBit(uint32 Idx) const;
	void SetBrickListBit(uint32 Idx, bool bValue);

	// Render thread
	struct
	{
		TRefCountPtr<IPooledRenderTarget> BrickAtlas;
		TRefCountPtr<FRDGPooledBuffer> PtrGrid;
		TRefCountPtr<FRDGPooledBuffer> BrickAllocCounter;
		uint32 LastBuildFrame = MAX_uint32;
		
		TRefCountPtr<FRDGPooledBuffer> BrickUpdateList;
		TArray<uint32> BrickListCPU;
	} RT;

}; // BrickmapBuilder