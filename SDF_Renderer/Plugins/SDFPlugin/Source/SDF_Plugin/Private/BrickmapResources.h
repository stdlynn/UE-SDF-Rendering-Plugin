#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "RenderGraphResources.h"

struct FBrickmapResources
{
	TRefCountPtr<IPooledRenderTarget> BrickAtlas;
	TRefCountPtr<FRDGPooledBuffer> BrickPtrBuffer;
	FRDGTextureDesc BrickAtlasDesc;
	FRDGBufferDesc BrickPtrBufferDesc;

	bool IsValid() const
	{
		return BrickAtlas.IsValid() && BrickPtrBuffer.IsValid();
	}

	void Release()
	{
		check(IsInRenderingThread());
		BrickAtlas.SafeRelease();
		BrickPtrBuffer.SafeRelease();
	}
};