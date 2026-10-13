// Custom SceneViewExtension Template for Unreal Engine
// Copyright 2023 - 2025 Ossi Luoto

#pragma once

// Plugin
#include "SDFEdits.h"
#include "SDFMaterial.h"
#include "FBrickmapBuilder.h"
#include "bvh.h"

// UE
#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "PixelShaderUtils.h"
#include "ScreenPass.h"
#include "SceneRendering.h"
#include "DataDrivenShaderPlatformInfo.h"

// Our Scene Extension
class FSDFExtension : public FSceneViewExtensionBase
{
public:

	struct FSettings
	{
		FTerrainParams Terrain;
		FVector3f PtrGridPos;
		float VoxSize;
		uint32 DbgVisMaxBricks;
		float DbgVisBlendFactor;
		float SurfaceEpsilon;
		float DerivativeH;
		float GPUUnitScale;
		bool bUnitScaleDirty{ true };
	};

	struct FSDFResources
	{
		TRefCountPtr<FRDGPooledBuffer> Buff{ nullptr };
		bool bDirty{ true }; // True to always allocate the buffer the first frame
		bool bUpload{ false };
	};

	FSDFExtension( const FAutoRegister& AutoRegister, UWorld* InWorld );

	virtual void SetupViewFamily( FSceneViewFamily& InViewFamily ) override {};
	virtual void SetupView( FSceneViewFamily& InViewFamily, FSceneView& InView ) override {};

	virtual void BeginRenderViewFamily( FSceneViewFamily& InViewFamily ) override;
	virtual void PreRenderViewFamily_RenderThread( FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily ) override;
	virtual void PostRenderBasePassDeferred_RenderThread( FRDGBuilder& GraphBuilder,
														  FSceneView& InView,
														  const FRenderTargetBindingSlots& RenderTargets,
														  TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures ) override;
	virtual void SubscribeToPostProcessingPass( EPostProcessingPass Pass, 
												const FSceneView& InView, 
												FPostProcessingPassDelegateArray& InOutPassCallbacks, 
												bool bIsPassEnabled ) override;
	FScreenPassTexture RenderDebug( FRDGBuilder& GraphBuilder, const FSceneView& SceneView, const FPostProcessMaterialInputs& Inputs );

	void ReleaseResources();
	void Reset();

protected:

	// To prevent multiple instances of the extension
	virtual bool IsActiveThisFrame_Internal( const FSceneViewExtensionContext& Context ) const override;

private:

	void TransferSettingsToRenderThread();
	void TransferSDFsToRenderThread();
	void UpdateSDFBuffer();
	void UpdateMaterialBuffer();
	void UpdateBVHBuffer();
	FRDGBufferRef RegisterSDFBuffer( FRDGBuilder& GraphBuilder );
	FRDGBufferRef RegisterMaterialBuffer( FRDGBuilder& GraphBuilder );

	// Game thread
	struct
	{
		TWeakObjectPtr<UWorld> World;
		FSettings Settings;
		TArray<FSDFMaterial> Materials;
	} GT{};

	// Render thread
	struct
	{
		FSettings Settings;
		TArray<FSDFMaterial> Materials;
		FSDFResources MaterialsResource;
		TArray<FGPUSDF> SDFs;
		FSDFResources SDFResources;
		FBrickmapBuilder BrickmapBuilder;
		FSDFBVH BVH;
		bool bDirtyBVH{ true };
	} RT{};

}; // FSDFExtension

// Traversal Shader
class SDFPLUGIN_API FSDFTraverseShader : public FGlobalShader
{
public:

	DECLARE_GLOBAL_SHADER( FSDFTraverseShader )

	SHADER_USE_PARAMETER_STRUCT( FSDFTraverseShader, FGlobalShader )

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_STRUCT_REF( FViewUniformShaderParameters, View ) // View/Camera
		SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer<FSDFMaterial>, Materials )
		SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer<FGPUSDF>, SDFEdits )
		SHADER_PARAMETER( uint32, NumEdits )
		SHADER_PARAMETER_RDG_TEXTURE_SRV( Texture3D<float>, BrickAtlas ) // Brick Atlas
		SHADER_PARAMETER_SAMPLER( SamplerState, BrickSampler ) // Brick Sampler
		SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer<uint>, MaterialAtlas ) // Material Atlas
		SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer<uint>, PtrGrid ) // Pointer Buffer
		SHADER_PARAMETER( uint32, bBruteForceSphereTracing )
		SHADER_PARAMETER( FVector3f, PtrGridPos )
		SHADER_PARAMETER( float, VoxSize )
		SHADER_PARAMETER( float, SurfaceEpsilon )
		SHADER_PARAMETER( float, DerivativeH )
		SHADER_PARAMETER( float, UnitScale )
		// Terrain
		SHADER_PARAMETER(FVector3f, RotAxis)
		SHADER_PARAMETER(float, RotAngle)
		SHADER_PARAMETER(FVector3f, PlanePos)
		SHADER_PARAMETER(float, VerticalNoiseOffset)
		SHADER_PARAMETER(float, MinNoiseScale)
		SHADER_PARAMETER(uint32, NumOctaves)
		SHADER_PARAMETER(float, Frequency)
		SHADER_PARAMETER(float, Lacunarity)
		SHADER_PARAMETER(float, Amplitude)
		SHADER_PARAMETER(float, Persistence)
		SHADER_PARAMETER(float, Inflation)
		SHADER_PARAMETER(float, InflationGain)
		SHADER_PARAMETER(float, SampleSmoothing)
		SHADER_PARAMETER(float, UnionSmoothing)
		SHADER_PARAMETER(float, ClippingSmoothing)
		SHADER_PARAMETER(float, PixelEpsilon)
		SHADER_PARAMETER(float, TerrainEpsilon)
		SHADER_PARAMETER(float, TerrainDerivativeH)
		SHADER_PARAMETER(uint32, TerrainMatIdx)
		SHADER_PARAMETER(uint32, bSimplex)
		SHADER_PARAMETER(uint32, bBandLimiting)
		RENDER_TARGET_BINDING_SLOTS() // GBuffer Textures
	END_SHADER_PARAMETER_STRUCT()

	// Basic shader initialization
	static bool ShouldCompilePermutation( const FGlobalShaderPermutationParameters& Parameters )
	{
		return IsFeatureLevelSupported( Parameters.Platform, ERHIFeatureLevel::SM6 );
	}

	// Define environment variables used by compute shader
	static void ModifyCompilationEnvironment( const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		//OutEnvironment.SetDefine(TEXT("SOME_VAR"), 1);
	}
};

// Brickmap Build Shader
class SDFPLUGIN_API FBuildBrickmapShader : public FGlobalShader
{
public:

	DECLARE_GLOBAL_SHADER(FBuildBrickmapShader)

	SHADER_USE_PARAMETER_STRUCT(FBuildBrickmapShader, FGlobalShader)

	class FTerrainPerm : SHADER_PERMUTATION_BOOL("TERRAIN");
	using FPermutationDomain = TShaderPermutationDomain<FTerrainPerm>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, BrickOffset)
		SHADER_PARAMETER(uint32, NumEdits)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUSDF>, SDFEdits)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(Texture3D<float>, BrickAtlas)
		SHADER_PARAMETER_RDG_BUFFER_UAV(Buffer<uint>, MaterialAtlas)
		SHADER_PARAMETER_RDG_BUFFER_UAV(Buffer<uint32>, PtrGrid)
		SHADER_PARAMETER(FIntVector, GridSize)
		SHADER_PARAMETER(FIntVector, AtlasSize)
		SHADER_PARAMETER(FVector3f, PtrGridPos)
		SHADER_PARAMETER(float, VoxSize)
		SHADER_PARAMETER(float, UnitScale)

		SHADER_PARAMETER(uint32, NumBricks)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint32>, UpdateList)
		SHADER_PARAMETER_RDG_BUFFER_UAV(Buffer<uint32>, FreedList)
		SHADER_PARAMETER_RDG_BUFFER_UAV(Buffer<uint32>, DeallocatedList)
		SHADER_PARAMETER_RDG_BUFFER_UAV(Buffer<uint32>, BrickAllocCounters)

		// Terrain
		SHADER_PARAMETER(FVector3f, RotAxis)
		SHADER_PARAMETER(float, RotAngle)
		SHADER_PARAMETER(FVector3f, PlanePos)
		SHADER_PARAMETER(float, VerticalNoiseOffset)
		SHADER_PARAMETER(float, MinNoiseScale)
		SHADER_PARAMETER(uint32, NumOctaves)
		SHADER_PARAMETER(float, Frequency)
		SHADER_PARAMETER(float, Lacunarity)
		SHADER_PARAMETER(float, Amplitude)
		SHADER_PARAMETER(float, Persistence)
		SHADER_PARAMETER(float, Inflation)
		SHADER_PARAMETER(float, InflationGain)
		SHADER_PARAMETER(float, SampleSmoothing)
		SHADER_PARAMETER(float, UnionSmoothing)
		SHADER_PARAMETER(float, ClippingSmoothing)
		SHADER_PARAMETER(float, PixelEpsilon)
		SHADER_PARAMETER(float, TerrainEpsilon)
		SHADER_PARAMETER(float, TerrainDerivativeH)
		SHADER_PARAMETER(uint32, bSimplex)
		SHADER_PARAMETER(uint32, bBandLimiting)
		SHADER_PARAMETER(uint32, TerrainMatIdx)
		SHADER_PARAMETER(uint32, TerrainMinBrick)
		SHADER_PARAMETER(uint32, TerrainMaxBrick)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADS_X"), 8);
		OutEnvironment.SetDefine(TEXT("THREADS_Y"), 8);
		OutEnvironment.SetDefine(TEXT("THREADS_Z"), 8);
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment); // Terrain macro
	}
};

// Transfer Deallocated Shader
class SDFPLUGIN_API FTransferDeallocatedShader : public FGlobalShader
{
public:

	DECLARE_GLOBAL_SHADER( FTransferDeallocatedShader )

	SHADER_USE_PARAMETER_STRUCT( FTransferDeallocatedShader, FGlobalShader )

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV( Buffer<uint32>, FreedList )
		SHADER_PARAMETER_RDG_BUFFER_UAV( Buffer<uint32>, DeallocatedList )
		SHADER_PARAMETER_RDG_BUFFER_UAV( Buffer<uint32>, BrickAllocCounters )
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation( const FGlobalShaderPermutationParameters& Parameters )
	{
		return IsFeatureLevelSupported( Parameters.Platform, ERHIFeatureLevel::SM5 );
	}
	static void ModifyCompilationEnvironment( const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment ) {}
};

// Reset Counters Shader
class SDFPLUGIN_API FResetCountersShader : public FGlobalShader
{
public:

	DECLARE_GLOBAL_SHADER( FResetCountersShader )

	SHADER_USE_PARAMETER_STRUCT( FResetCountersShader, FGlobalShader )

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV( Buffer<uint32>, FreedList )
		SHADER_PARAMETER_RDG_BUFFER_UAV( Buffer<uint32>, DeallocatedList )
		SHADER_PARAMETER_RDG_BUFFER_UAV( Buffer<uint32>, BrickAllocCounters )
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation( const FGlobalShaderPermutationParameters& Parameters )
	{
		return IsFeatureLevelSupported( Parameters.Platform, ERHIFeatureLevel::SM5 );
	}
	static void ModifyCompilationEnvironment( const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment ) {}
};

// Debug Shader
class SDFPLUGIN_API FDebugShader : public FGlobalShader
{
public:

	DECLARE_GLOBAL_SHADER( FDebugShader )

	SHADER_USE_PARAMETER_STRUCT( FDebugShader, FGlobalShader )

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_STRUCT( FScreenPassTextureViewportParameters, SceneColorViewport )
		SHADER_PARAMETER_RDG_TEXTURE( Texture2D, OriginalSceneColor )
		SHADER_PARAMETER_RDG_TEXTURE( Texture2D, DepthTex )
		SHADER_PARAMETER_RDG_TEXTURE_UAV( RWTexture2D<float4>, Output )
		SHADER_PARAMETER_STRUCT_REF( FViewUniformShaderParameters, View )
		SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer<uint>, PtrGrid )
		SHADER_PARAMETER( FVector3f, PtrGridPos )
		SHADER_PARAMETER( float, VoxSize )
		SHADER_PARAMETER( float, UnitScale )
		SHADER_PARAMETER( uint32, DbgVisMaxBricks )
		SHADER_PARAMETER( float, DbgVisBlendFactor )
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation( const FGlobalShaderPermutationParameters& Parameters )
	{
		return IsFeatureLevelSupported( Parameters.Platform, ERHIFeatureLevel::SM6 );
	}
};