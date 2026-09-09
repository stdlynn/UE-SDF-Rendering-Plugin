// Custom SceneViewExtension Template for Unreal Engine
// Copyright 2023 - 2025 Ossi Luoto

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphUtils.h"
#include "SceneViewExtension.h"
#include "PixelShaderUtils.h"
#include "ScreenPass.h"
#include "SceneRendering.h"
#include "DataDrivenShaderPlatformInfo.h"

// Our Scene Extension
class FSDFSceneViewExtension : public FSceneViewExtensionBase
{
public:

	FSDFSceneViewExtension(const FAutoRegister& AutoRegister);

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {};
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;

	// See SceneViewExtension.h for hooks to different stages of rendering
	virtual void PostRenderBasePassDeferred_RenderThread( FRDGBuilder& GraphBuilder, 
														  FSceneView& InView, 
														  const FRenderTargetBindingSlots& RenderTargets, 
														  TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override;

private:

	void UploadSDFs( const UWorld* World );

	TArray<FVector4f> SDFRenderData_RenderThread;

};

// Custom Shader
class SDFPLUGIN_API FSDFTraverseShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSDFTraverseShader)

	SHADER_USE_PARAMETER_STRUCT(FSDFTraverseShader, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View) // View/Camera
		RENDER_TARGET_BINDING_SLOTS() // RenderTarget Textures
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