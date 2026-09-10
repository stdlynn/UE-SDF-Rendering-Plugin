// Custom SceneViewExtension Template for Unreal Engine
// Copyright 2023 - 2025 Ossi Luoto

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "PixelShaderUtils.h"
#include "ScreenPass.h"
#include "SceneRendering.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "BrickmapResources.h"

// Our Scene Extension
class FSDFSceneViewExtension : public FSceneViewExtensionBase
{
public:
	FSDFSceneViewExtension(const FAutoRegister& AutoRegister, UWorld* InWorld);

	// To prevent multiple instaces of the extension
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;
	
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {};
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {};

	void AllocateBrickmap();
	void ReleaseResources();
	
	// See SceneViewExtension.h for hooks to different stages of rendering
	// PreRenderViewFamily is called before rendering of each view family
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	// PostRenderBasePassDeferred is called after rendering but before deferred lighting
	virtual void PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override;

private:
	TWeakObjectPtr<UWorld> World;
	FBrickmapResources BrickmapResources;
	uint32 LastBrickmapBuildFrame = MAX_uint32;
};

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
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D<float>, BrickAtlas)	// Brick Atlas
		SHADER_PARAMETER_SAMPLER(SamplerState, BrickSampler)			// Brick Sampler
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, PtrBuffer)		// Pointer Buffer
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
		SHADER_PARAMETER_RDG_TEXTURE_UAV(Texture3D<float>, BrickAtlas)	 // Brick Atlas
		SHADER_PARAMETER_RDG_BUFFER_UAV(Buffer<uint>, PtrGrid)			 // Pointer Grid
		SHADER_PARAMETER_RDG_BUFFER_UAV(Buffer<uint>, BrickAllocCounter) // Atomic Counter
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
