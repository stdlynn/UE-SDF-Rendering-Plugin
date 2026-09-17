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

	FBrickmapBuilder() = default;

	[[nodiscard]] IPooledRenderTarget* GetBrickAtlas() const { return RT.BrickAtlas; }
	[[nodiscard]] FRDGPooledBuffer* GetPtrGrid() const { return RT.PtrGrid; }
	[[nodiscard]] FRDGPooledBuffer* GetBrickAllocCounter() const { return RT.BrickAllocCounters; }
	[[nodiscard]] uint32 GetLastBuildFrame() const { return RT.LastBuildFrame; }
	[[nodiscard]] uint32 GetNumUpdatedBricks() const { return RT.UpdateListCPU.Num(); }
	void SetLastBuildFrame(const uint32 Frame) { RT.LastBuildFrame = Frame; }

	void AllocateResources();
	void ReleaseResources();
	bool AreResourcesValid() const;

	void SetBrickUpdateList(const TArray<FGPUSDF>& SDFs);
	void AllocateBrickList();
	FRDGBufferRef RegisterBrickListBuffer(FRDGBuilder& GraphBuilder) const;

private:

	//[[nodiscard]] bool GetBrickListBit(uint32 Idx) const;
	//void SetBrickListBit(uint32 Idx, bool bValue);

	// Render thread
	struct
	{
		uint32 LastBuildFrame = MAX_uint32;
		TRefCountPtr<IPooledRenderTarget> BrickAtlas;
		TRefCountPtr<FRDGPooledBuffer> PtrGrid;

		TSet<uint32> UpdateListCPU;
		TRefCountPtr<FRDGPooledBuffer> UpdateListResource;
		TRefCountPtr<FRDGPooledBuffer> BrickAllocCounters;
		TRefCountPtr<FRDGPooledBuffer> FreedListResource;
		TRefCountPtr<FRDGPooledBuffer> DeallocatedListResource;

	} RT;

}; // BrickmapBuilder