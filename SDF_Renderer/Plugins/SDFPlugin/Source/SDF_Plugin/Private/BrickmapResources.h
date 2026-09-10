#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "RenderGraphResources.h"

struct FBrickmapResources
{
	TRefCountPtr<IPooledRenderTarget> BrickAtlas;
	TRefCountPtr<FRDGPooledBuffer> BrickPtrBuffer;
	TRefCountPtr<FRDGPooledBuffer> BrickAllocCounter;
	// Store descs to be able to query their dimensions
	FRDGTextureDesc BrickAtlasDesc;
	FRDGBufferDesc BrickPtrBufferDesc;

	bool IsValid() const
	{
		return BrickAtlas.IsValid() && BrickPtrBuffer.IsValid()
				&& BrickAllocCounter.IsValid();
	}

	void Release()
	{
		check(IsInRenderingThread());
		BrickAtlas.SafeRelease();
		BrickPtrBuffer.SafeRelease();
		BrickAllocCounter.SafeRelease();
	}
};