// Custom SceneViewExtension Template for Unreal Engine
// Copyright 2023 - 2025 Ossi Luoto

#include "SDFSceneViewExtension.h"
#include "PostProcess/SceneRenderTargets.h"

IMPLEMENT_GLOBAL_SHADER(FSDFTraverseShader, "/Plugins/SDFPlugin/PostProcessCS.usf", "MainPS", SF_Pixel);

namespace
{
	TAutoConsoleVariable<int32> CVarShaderOn(
		TEXT("r.SDFPlugin"),
		1,
		TEXT("Enable SDF Plugin \n")
		TEXT(" 0: OFF;")
		TEXT(" 1: ON."),
		ECVF_RenderThreadSafe);
}

// Extension initialization
FSDFSceneViewExtension::FSDFSceneViewExtension(const FAutoRegister& AutoRegister) 
	: FSceneViewExtensionBase(AutoRegister)
{
	UE_LOG(LogTemp, Log, TEXT("SDFPlugin: Custom SceneViewExtension registered"));
}

// PostRenderBasePassDeferred is called after rendering but before deferred lighting
void FSDFSceneViewExtension::PostRenderBasePassDeferred_RenderThread( FRDGBuilder& GraphBuilder, 
																	  FSceneView& InView, 
																	  const FRenderTargetBindingSlots& RenderTargets, 
																	  TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
	const FRDGTextureRef Depth = RenderTargets.DepthStencil.GetTexture();
	if (!Depth)
	{
		UE_LOG(LogTemp, Error, TEXT("No Depth Stencil texture available for Rendering"));
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "SDF_Trace");
	{
		// Gives us more data from the FSceneView
		const FViewInfo& View = static_cast<const FViewInfo&>(InView);

		FSDFTraverseShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FSDFTraverseShader::FParameters>();
		PassParameters->View = InView.ViewUniformBuffer;
		PassParameters->RenderTargets = RenderTargets;
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
			Depth,
			ERenderTargetLoadAction::ELoad, // Depth
			ERenderTargetLoadAction::ELoad, // Stencil
			FExclusiveDepthStencil::DepthWrite_StencilWrite);

		FRHIDepthStencilState* DepthStencilState = TStaticDepthStencilState<
			true, // bEnableDepthTest
			CF_DepthNearOrEqual,
			true, // bEnableFrontFaceStencil
			CF_Always, SO_Keep, SO_Keep, SO_Replace,
			false, // bEnableBackFaceStencil
			CF_Always, SO_Keep, SO_Keep, SO_Keep,
			0x0, // StencilReadMask
			0x0>::GetRHI();

		const TShaderMapRef<FSDFTraverseShader> PixelShader(View.ShaderMap);

		// For if we ever need to write to the stencil buffer
		constexpr uint32 StencilRef =
			GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1) |
			STENCIL_LIGHTING_CHANNELS_MASK(0x1);

		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder,
			View.ShaderMap,
			RDG_EVENT_NAME("SDF_Trace"),
			PixelShader,
			PassParameters,
			View.ViewRect,
			TStaticBlendState<>::GetRHI(), // Default blend state
			TStaticRasterizerState<FM_Solid, CM_None>::GetRHI(), // Fill mode, Cull Mode
			DepthStencilState,
			StencilRef);
	}
}