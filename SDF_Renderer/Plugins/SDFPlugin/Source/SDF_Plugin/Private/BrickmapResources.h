//#pragma once
//
//#include "CoreMinimal.h"
//#include "RendererInterface.h"
//#include "RenderGraphResources.h"
//
//struct FBrickmapResources
//{
//	TRefCountPtr<IPooledRenderTarget> BrickAtlas;
//	TRefCountPtr<FRDGPooledBuffer> PtrGrid;
//	TRefCountPtr<FRDGPooledBuffer> BrickAllocCounter;
//
//	bool IsValid() const
//	{
//		return BrickAtlas.IsValid() && PtrGrid.IsValid()
//				&& BrickAllocCounter.IsValid();
//	}
//
//	void Release()
//	{
//		check(IsInRenderingThread());
//		BrickAtlas.SafeRelease();
//		PtrGrid.SafeRelease();
//		BrickAllocCounter.SafeRelease();
//	}
//};