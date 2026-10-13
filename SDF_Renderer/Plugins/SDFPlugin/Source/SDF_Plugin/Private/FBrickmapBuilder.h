#pragma once

// Plugin
#include "SDFEdits.h"

// UE
#include "CoreMinimal.h"
#include "PixelShaderUtils.h"
#include "DataDrivenShaderPlatformInfo.h"

struct FTerrainParams
{
	FVector3f RotAxis;
	float RotAngle;
	FVector3f PlanePos;
	float VerticalNoiseOffset;
	float MinNoiseScale;
	uint32 NumOctaves;
	float Frequency;
	float Lacunarity;
	float Amplitude;
	float Persistence;
	float Inflation;
	float InflationGain;
	float SampleSmoothing;
	float UnionSmoothing;
	float ClippingSmoothing;
	float PixelEpsilon;
	float Epsilon;
	float DerivativeH;
	bool bSimplex;
	bool bBandLimiting;
	uint32 MatIdx;
};

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
	[[nodiscard]] static float GetTerrainHeightMin( const FTerrainParams& Params );
	[[nodiscard]] static float GetTerrainHeightMax( const FTerrainParams& Params );
	[[nodiscard]] static uint32 GetTerrainBrickMin( const FTerrainParams& Params, float VoxSize, float PtrGridZ );
	[[nodiscard]] static uint32 GetTerrainBrickMax( const FTerrainParams& Params, float VoxSize, float PtrGridZ );
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