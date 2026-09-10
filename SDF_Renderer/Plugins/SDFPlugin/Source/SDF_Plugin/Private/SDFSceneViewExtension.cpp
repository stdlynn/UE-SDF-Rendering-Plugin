// Custom SceneViewExtension Template for Unreal Engine
// Copyright 2023 - 2025 Ossi Luoto

// Plugin
#include "SDFSceneViewExtension.h"
#include "PostProcess/SceneRenderTargets.h"

// UE
#include "EngineUtils.h"

IMPLEMENT_GLOBAL_SHADER( FSDFTraverseShader, "/Plugins/SDFPlugin/PostProcessCS.usf", "MainPS", SF_Pixel );

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

void FSDFSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	// 'BeginRenderViewFamily' is called when the view family is about to be rendered
	// Allowing for game thread work before rendering a view family
	// A view represents a single viewpoint (camera) that we render from
	// A view family is a group of views that gets rendered together for the same frame/setup

	// We call this from here so it runs on the Game Thread
	TransferSDFsToRenderThread( InViewFamily.Scene->GetWorld() );
};

void FSDFSceneViewExtension::TransferSDFsToRenderThread( const UWorld* World )
{
	// Only update if dirty
	USDFSubsystem* SDFSubsystem{ World->GetSubsystem<USDFSubsystem>() };
	if (not SDFSubsystem->GetAndResetDirty()) { return; }

	// Debug
	GEngine->AddOnScreenDebugMessage( -1, // Force newline
									  0.2f, // Time
									  FColor::Yellow, 
									  FString::Printf( TEXT( "Dirty SDF(s) found, updating render thread..." ) ) );

	// Grab the game thread's SDFs
	const TMap<uint64, FSDFRenderParams>& Params{ SDFSubsystem->GetRenderParams() };

	// Transfer lambda
	// Capture the parameters by copy, their state on the game thread might change before the lambda runs
	auto TransferParams{ [this, Params = Params]( FRHICommandList& ) mutable
	{
		// On the render thread, set the SDF render data
		RT.SDFRenderParams = MoveTemp( Params );
		RT.SDFEdits.bDirty = true;
	} };

	// Perform the transfer safely on the render thread
	ENQUEUE_RENDER_COMMAND( SDFParamsThreadTransfer )( TransferParams );
}

void FSDFSceneViewExtension::PreRenderViewFamily_RenderThread( FRDGBuilder& GraphBuilder, 
															   FSceneViewFamily& InViewFamily )
{
	CreateSDFBuffer();
}

void FSDFSceneViewExtension::CreateSDFBuffer()
{
	// No SDF edits have been allocated on the GPU
	if (not RT.SDFEdits.Buff.IsValid())
	{
		const uint32 NumAlloc{ static_cast<uint32>(std::max( RT.SDFRenderParams.Num(), 1 )) };
		const FRDGBufferDesc Desc{ FRDGBufferDesc::CreateStructuredDesc( sizeof( FSDFRenderParams ), NumAlloc ) };
		RT.SDFEdits.Buff = AllocatePooledBuffer( Desc, TEXT( "SDF Buffer" ) );
	}
	// There is an existing buffer on the GPU, but we need to update it
	else if (RT.SDFEdits.bDirty)
	{
		// We can update/overwrite the current SDF edits
		const uint32 CurrNum{ RT.SDFEdits.Buff->GetSize() / sizeof( FSDFRenderParams ) };
		if (CurrNum >= static_cast<uint32>(RT.SDFRenderParams.Num()))
		{
			// TODO: upload new data
		}
		// There isn't enough space to store the SDF edits, reallocate
		else
		{
			// TODO: dealloc old buff (or resize?)
			// TODO: alloc new buff
		}
	}
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

		// TODO: register every frame?
		// The SDF edits buffer is persistent
		const FRDGBufferRef RDGBuff = GraphBuilder.RegisterExternalBuffer( RT.SDFEdits.Buff, ERDGBufferFlags::MultiFrame );

		FSDFTraverseShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FSDFTraverseShader::FParameters>();
		PassParameters->View = InView.ViewUniformBuffer;
		PassParameters->SDFEdits = GraphBuilder.CreateSRV( RDGBuff );
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