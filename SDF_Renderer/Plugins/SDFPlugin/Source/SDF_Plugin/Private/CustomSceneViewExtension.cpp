// Custom SceneViewExtension Template for Unreal Engine
// Copyright 2023 - 2025 Ossi Luoto
// 
// Custom SceneViewExtension implementation

#include "CustomSceneViewExtension.h"
#include "PostProcess/SceneRenderTargets.h"

IMPLEMENT_GLOBAL_SHADER(FCustomShader, "/Plugins/SDFPlugin/PostProcessCS.usf", "MainPS", SF_Pixel);

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

FCustomSceneViewExtension::FCustomSceneViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister)
{
	UE_LOG(LogTemp, Log, TEXT("SDFPlugin: Custom SceneViewExtension registered"));
}

void FCustomSceneViewExtension::PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
	FRDGTextureRef Depth = RenderTargets.DepthStencil.GetTexture();
	if (!Depth)
	{
		UE_LOG(LogTemp, Error, TEXT("No Depth Stencil texture available for Rendering"));
		return;
	}

	const FViewInfo& View = static_cast<const FViewInfo&>(InView);

	// Here starts the RDG stuff
	RDG_EVENT_SCOPE(GraphBuilder, "SDF_Trace");
	{
		int32 NumTargets = 0;
		for (int32 i = 0; i < MaxSimultaneousRenderTargets; ++i)
		{
			if (RenderTargets.Output[i].GetTexture())
			{
				NumTargets = i + 1;
			}
		}

		bool bGBufferHasClearCoat = (NumTargets >= 6);
		bool bWriteVelocity = (NumTargets >= 5) && 
			FCString::Strifind(RenderTargets.Output[NumTargets - 1].GetTexture()->Name, 
			TEXT("Velocity")) != nullptr;

		FCustomShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FCustomShader::FParameters>();
		PassParameters->View = InView.ViewUniformBuffer;
		PassParameters->RenderTargets = RenderTargets;
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
			Depth,
			ERenderTargetLoadAction::ELoad, // depth
			ERenderTargetLoadAction::ELoad, // stencil
			FExclusiveDepthStencil::DepthWrite_StencilWrite);

		FRHIDepthStencilState* DepthStencilState = TStaticDepthStencilState<
			true,
			CF_DepthNearOrEqual,
			true,
			CF_Always, SO_Keep, SO_Keep, SO_Replace,
			false,
			CF_Always, SO_Keep, SO_Keep, SO_Keep,
			0x0,
			0x0>::GetRHI();

		// If we need shader permutations for different GBuffer setups
		//TShaderMapRef<FCustomShader> PixelShader(
		//	View.ShaderMap,
		//	[&] {
		//		FCustomShader::FPermutationDomain Perm;
		//		Perm.Set<FCustomShader::FWriteVelocity>(bWriteVelocity);
		//		Perm.Set<FCustomShader::FHasGBufferE>(bHasGBufferE);
		//		return Perm;
		//	}());
		TShaderMapRef<FCustomShader> PixelShader(View.ShaderMap);

		const uint32 StencilRef =
			GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1) |
			STENCIL_LIGHTING_CHANNELS_MASK(0x1);

		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder,
			View.ShaderMap,
			RDG_EVENT_NAME("SDF_Trace"),
			PixelShader,
			PassParameters,
			View.ViewRect,
			TStaticBlendState<>::GetRHI(),
			TStaticRasterizerState<FM_Solid, CM_None>::GetRHI(),
			DepthStencilState,
			StencilRef);
	}
}