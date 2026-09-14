#pragma once

// Plugin
#include "SDFEdits.h"

// UE
#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "PixelShaderUtils.h"
#include "ScreenPass.h"
#include "SceneRendering.h"
#include "DataDrivenShaderPlatformInfo.h"

static const FIntVector AtlasSize = FIntVector(512, 512, 8);
static const FIntVector PtrGridSize = FIntVector(64, 64, 64);

class FBrickmapBuilder
{
public:

	FBrickmapBuilder() = default;

	[[nodiscard]] IPooledRenderTarget* GetBrickAtlas() const { return RT.BrickAtlas; }
	[[nodiscard]] FRDGPooledBuffer* GetPtrGrid() const { return RT.PtrGrid; }
	[[nodiscard]] FRDGPooledBuffer* GetBrickAllocCounter() const { return RT.BrickAllocCounter; }
	[[nodiscard]] uint32 GetLastBuildFrame() const { return RT.LastBuildFrame; }
	void SetLastBuildFrame(const uint32 Frame) { RT.LastBuildFrame = Frame; }

	void AllocateResources();
	void ReleaseResources();
	bool AreResourcesValid() const;


private:

	// Render thread
	struct
	{
		TRefCountPtr<IPooledRenderTarget> BrickAtlas;
		TRefCountPtr<FRDGPooledBuffer> PtrGrid;
		TRefCountPtr<FRDGPooledBuffer> BrickAllocCounter;
		uint32 LastBuildFrame = MAX_uint32;
	} RT;

}; // BrickmapBuilder