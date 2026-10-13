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
	[[nodiscard]] FRDGPooledBuffer* GetFreedList() const { return RT.FreedListResource; }
	[[nodiscard]] FRDGPooledBuffer* GetDeallocatedList() const { return RT.DeallocatedListResource; }
	[[nodiscard]] uint32 GetLastBuildFrame() const { return RT.LastBuildFrame; }
	[[nodiscard]] uint32 GetNumUpdatedBricks() const { return RT.UpdateListCPU.Num(); }
	void SetLastBuildFrame( const uint32 Frame ) { RT.LastBuildFrame = Frame; }

	void AllocateResources();
	void ReleaseResources();
	bool AreResourcesValid() const;

	// Useful for removing stale cached geometry
	void ClearBrickmap(FRDGBuilder& GraphBuilder );

	// All variables passed should have the GPU unit size conversions applied
	void SetBrickUpdateList( const TArray<FGPUSDF>& SDFs, const FVector3f& PtrGridPos, float VoxSize );
	void AllocateBrickList();
	FRDGBufferRef RegisterBrickListBuffer( FRDGBuilder& GraphBuilder ) const;

	bool bReset = true;
	bool bTerrainDirty = true;

private:

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