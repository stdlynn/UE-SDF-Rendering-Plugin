// Custom SceneViewExtension Template for Unreal Engine
// Copyright 2023 - 2025 Ossi Luoto

#pragma once

// Plugin
#include "SDFEdits.h"
#include "BrickmapResources.h"

// UE
#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "PixelShaderUtils.h"
#include "ScreenPass.h"
#include "SceneRendering.h"
#include "DataDrivenShaderPlatformInfo.h"

// TODO: once the sun and moon align (and our branches), rename to FSDFExtension
// TODO: also rename file
// Our Scene Extension
class FSDFExtension : public FSceneViewExtensionBase
{
public:

	struct FSDFResources
	{
		// TODO: also a uniform with 'NumEdits'
		TRefCountPtr<FRDGPooledBuffer> Buff{ nullptr };
		bool bDirty{ true }; // True to always allocate the buffer the first frame
		bool bUpload{ false };
	};

	FSDFExtension(const FAutoRegister& AutoRegister, UWorld* InWorld);
	
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {};

	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual void PostRenderBasePassDeferred_RenderThread( FRDGBuilder& GraphBuilder, 
														  FSceneView& InView, 
														  const FRenderTargetBindingSlots& RenderTargets, 
														  TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override;
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& InView, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
	FScreenPassTexture RenderDebug(FRDGBuilder& GraphBuilder, const FSceneView& SceneView, const FPostProcessMaterialInputs& Inputs);

	void AllocateBrickmap();
	void ReleaseResources();

protected:

	// To prevent multiple instances of the extension
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:

	void TransferSDFsToRenderThread();
	void UpdateSDFBuffer();
	FRDGBufferRef RegisterSDFBuffer(FRDGBuilder& GraphBuilder);
		
	// Game thread
	struct
	{
		TWeakObjectPtr<UWorld> World;
	} GT;

	// Render thread
	struct
	{
		TArray<FGPUSDF> SDFs;
		FSDFResources SDFResources;
		FBrickmapResources BrickmapResources;
		uint32 LastBrickmapBuildFrame = MAX_uint32;
	} RT;

}; // FSDFExtension

BEGIN_UNIFORM_BUFFER_STRUCT(FPtrBuffer, )
	SHADER_PARAMETER(uint32, Idx)
END_UNIFORM_BUFFER_STRUCT()

BEGIN_UNIFORM_BUFFER_STRUCT(FBrickAllocCounter, )
	SHADER_PARAMETER(uint32, Counter)
END_UNIFORM_BUFFER_STRUCT()

// Traversal Shader
class SDFPLUGIN_API FSDFTraverseShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSDFTraverseShader)

	SHADER_USE_PARAMETER_STRUCT(FSDFTraverseShader, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View) // View/Camera
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUSDF>, SDFEdits)
		SHADER_PARAMETER(uint32, NumEdits)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D<float>, BrickAtlas)	// Brick Atlas
		SHADER_PARAMETER_SAMPLER(SamplerState, BrickSampler)			// Brick Sampler
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, PtrGrid)		// Pointer Buffer
		RENDER_TARGET_BINDING_SLOTS() // GBuffer Textures
	END_SHADER_PARAMETER_STRUCT()

	// Basic shader initialization
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	// Define environment variables used by compute shader
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
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

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(Texture3D<float>, BrickAtlas)	   // Brick Atlas
		SHADER_PARAMETER_RDG_BUFFER_UAV(Buffer<uint32>, PtrGrid)		   // Pointer Grid
		SHADER_PARAMETER_RDG_BUFFER_UAV(Buffer<uint32>, BrickAllocCounter) // Atomic Counter
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUSDF>, SDFEdits)
		SHADER_PARAMETER(uint32, NumEdits)
		SHADER_PARAMETER(FIntVector, GridSize)
		SHADER_PARAMETER(FIntVector, AtlasSize)
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
	}
};

// Debug Shader
class SDFPLUGIN_API FDebugShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FDebugShader)

	SHADER_USE_PARAMETER_STRUCT(FDebugShader, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, SceneColorViewport)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OriginalSceneColor)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Output)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, PtrGrid)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
