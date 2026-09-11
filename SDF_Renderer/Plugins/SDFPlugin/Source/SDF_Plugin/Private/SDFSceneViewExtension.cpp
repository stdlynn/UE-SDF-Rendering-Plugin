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
		TEXT( "r.SDFPlugin" ),
		1,
		TEXT( "Enable SDF Plugin \n" )
		TEXT( " 0: OFF;" )
		TEXT( " 1: ON." ),
		ECVF_RenderThreadSafe );
}

// Extension initialization
FSDFSceneViewExtension::FSDFSceneViewExtension( const FAutoRegister& AutoRegister )
	: FSceneViewExtensionBase( AutoRegister )
{
	UE_LOG( LogTemp, Log, TEXT("SDFPlugin: Custom SceneViewExtension registered") );
}

void FSDFSceneViewExtension::BeginRenderViewFamily( FSceneViewFamily& InViewFamily )
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

	// TODO: remove before PR (also other debug messages)
	// Debug
	GEngine->AddOnScreenDebugMessage( -1, // Force newline
									  0.2f, // Time
									  FColor::Yellow,
									  FString::Printf( TEXT( "Dirty SDF(s) found, updating render thread..." ) ) );

	// Grab the game thread's SDFs
	const auto& Params{ SDFSubsystem->GetRenderParams() };

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
	CreateSDFBuffer( GraphBuilder );
}

void FSDFSceneViewExtension::CreateSDFBuffer( FRDGBuilder& GraphBuilder )
{
	// No SDF edits have been allocated on the GPU
	if (not RT.SDFResources.Buff.IsValid())
	{
		// We cannot allocate a buffer of size 0
		const uint32 NumAlloc{ static_cast<uint32>(FMath::Max( RT.SDFs.Num(), 1 )) };

		// We need a structured buffer (because we're storing an array of structs)
		// We need it to be an upload buffer, which are for CPU-to-GPU uploads
		const FRDGBufferDesc Desc{ FRDGBufferDesc::CreateStructuredUploadDesc( sizeof( FRenderSDF ), NumAlloc ) }

		// By manually allocating a buffer, we guarantee it's persistent among frames
		// This wouldn't be the case for 'GraphBuilder.CreateBuffer()'
		RT.SDFResources.Buff = AllocatePooledBuffer( Desc, TEXT( "SDF Upload Buffer" ) );


		FRDGUploadData<FRenderSDF> UploadData{ GraphBuilder, NumEdits };
		// TODO: can also provide a data array (e.g. from storing it in a TArray) to the 2nd arg of QueueBufferUpload
		// TODO: I put this as the second QueueBufferUpload, commented out
		for (uint64 i{ 0 }; i < NumEdits; ++i)
		{
			UploadData[i] =;
		}
		// TODO: NoCopy flag to indicate the uploaded data (as owned by us) will persist until graph deferral
		// TODO: upload here or in RDG_EVENT_SCOPE?
		GraphBuilder.QueueBufferUpload( SDFBuffRDG, UploadData, ERDGInitialDataFlags::NoCopy );
	}
	// There is an existing buffer on the GPU, but we need to update it
	else if (RT.SDFResources.bDirty)
	{
		// We can update/overwrite the current SDF edits
		const uint32 CurrNum{ RT.SDFEdits.Buff->GetSize() / sizeof( FRenderSDF ) };
		if (CurrNum >= static_cast<uint32>(RT.SDFRenderParams.Num()))
		{
			// TODO: upload new data
		}
		// There isn't enough space to store the SDF edits, reallocate
		else
		{
			// TODO: dealloc old buff (or resize?)
			// TODO: Don't think so, it's a shared pointer
			// TODO: if you assign something new to it, the old one goes out of scope
			// TODO: the RDG *should* take care of it (verify)
			// TODO:
			// TODO: alloc new buff
		}
	}
}

// PostRenderBasePassDeferred is called after rendering but before deferred lighting
void FSDFSceneViewExtension::PostRenderBasePassDeferred_RenderThread( FRDGBuilder& GraphBuilder,
																	  FSceneView& InView,
																	  const FRenderTargetBindingSlots& RenderTargets,
																	  TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures )
{
	
	// Create the SDF buffer view (which specifies how the shader should read it)
	GraphBuilder.CreateSRV();

	const FRDGTextureRef Depth = RenderTargets.DepthStencil.GetTexture();
	if (! Depth)
	{
		UE_LOG( LogTemp, Error, TEXT("No Depth Stencil texture available for Rendering") );
		return;
	}

	RDG_EVENT_SCOPE( GraphBuilder, "SDF_Trace" );
	{
		// Gives us more data from the FSceneView
		const FViewInfo& View = static_cast<const FViewInfo&>(InView);

		// Tell the RDG that the SDF buffer will be used this frame
		// Specify that it is persistent/external ('MultiFrame')
		GraphBuilder.RegisterExternalBuffer( RT.SDFResources.Buff, ERDGBufferFlags::MultiFrame );

		FSDFTraverseShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FSDFTraverseShader::FParameters>();
		PassParameters->View = InView.ViewUniformBuffer;
		PassParameters->SDFEdits = GraphBuilder.CreateSRV(  );
		PassParameters->RenderTargets = RenderTargets;
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
			Depth,
			ERenderTargetLoadAction::ELoad, // Depth
			ERenderTargetLoadAction::ELoad, // Stencil
			FExclusiveDepthStencil::DepthWrite_StencilWrite );

		FRHIDepthStencilState* DepthStencilState = TStaticDepthStencilState<
			true, // bEnableDepthTest
			CF_DepthNearOrEqual,
			true, // bEnableFrontFaceStencil
			CF_Always, SO_Keep, SO_Keep, SO_Replace,
			false, // bEnableBackFaceStencil
			CF_Always, SO_Keep, SO_Keep, SO_Keep,
			0x0, // StencilReadMask
			0x0>::GetRHI();

		const TShaderMapRef<FSDFTraverseShader> PixelShader( View.ShaderMap );

		// For if we ever need to write to the stencil buffer
		constexpr uint32 StencilRef =
				GET_STENCIL_BIT_MASK( RECEIVE_DECAL, 1 ) |
				STENCIL_LIGHTING_CHANNELS_MASK( 0x1 );

		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder,
			View.ShaderMap,
			RDG_EVENT_NAME( "SDF_Trace" ),
			PixelShader,
			PassParameters,
			View.ViewRect,
			TStaticBlendState<>::GetRHI(), // Default blend state
			TStaticRasterizerState<FM_Solid, CM_None>::GetRHI(), // Fill mode, Cull Mode
			DepthStencilState,
			StencilRef );
	}
}