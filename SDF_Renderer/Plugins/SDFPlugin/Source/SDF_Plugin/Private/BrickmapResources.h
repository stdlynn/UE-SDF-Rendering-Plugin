#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "RenderGraphResources.h"

struct FBrickmapResources
{
	TRefCountPtr<IPooledRenderTarget> BrickAtlas;
	TRefCountPtr<FRDGPooledBuffer> PtrBuffer;
	TRefCountPtr<FRDGPooledBuffer> BrickAllocCounter;
	// Store descs to be able to query their dimensions
	FRDGTextureDesc BrickAtlasDesc;
	FRDGBufferDesc PtrGridDesc;

	bool IsValid() const
	{
		return BrickAtlas.IsValid() && PtrBuffer.IsValid()
				&& BrickAllocCounter.IsValid();
	}

	void Release()
	{
		check(IsInRenderingThread());
		BrickAtlas.SafeRelease();
		PtrBuffer.SafeRelease();
		BrickAllocCounter.SafeRelease();
	}
};