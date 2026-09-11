// Custom SceneViewExtension Template for Unreal Engine
// Copyright 2023 - 2025 Ossi Luoto

#pragma once

// Plugin
#include "SDFEdits.h"

// UE
#include "CoreMinimal.h"
#include "RenderGraphUtils.h"
#include "SceneViewExtension.h"
#include "PixelShaderUtils.h"
#include "ScreenPass.h"
#include "SceneRendering.h"
#include "DataDrivenShaderPlatformInfo.h"

// TODO: once the sun and moon align (and our branches), rename to FSDFExtension
// TODO: also rename file
// Our Scene Extension
class FSDFSceneViewExtension : public FSceneViewExtensionBase
{
public:

	struct FSDFResources
	{
		// TODO: also a uniform with 'NumEdits'
		TRefCountPtr<FRDGPooledBuffer> Buff{ nullptr };
		bool bDirty{ true }; // True to always allocate the buffer the first frame
		bool bUpload{ false };
	};

	FSDFSceneViewExtension(const FAutoRegister& AutoRegister);

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {};

	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual void PostRenderBasePassDeferred_RenderThread( FRDGBuilder& GraphBuilder, 
														  FSceneView& InView, 
														  const FRenderTargetBindingSlots& RenderTargets, 
														  TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override;

private:

	void TransferSDFsToRenderThread( const UWorld* World );
	void UpdateSDFBuffer();
		
	// Game thread
	//struct
	//{
	//	
	//} GT;

	// Render thread
	struct
	{
		// TODO: make TArray
		TArray<FGPUSDF> SDFs;
		FSDFResources SDFResources;
	} RT;

}; // FSDFSceneViewExtension

// Custom Shader
class SDFPLUGIN_API FSDFTraverseShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSDFTraverseShader)

	SHADER_USE_PARAMETER_STRUCT(FSDFTraverseShader, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View) // View/Camera
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUSDF>, SDFEdits)
		SHADER_PARAMETER(uint32, NumEdits)
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