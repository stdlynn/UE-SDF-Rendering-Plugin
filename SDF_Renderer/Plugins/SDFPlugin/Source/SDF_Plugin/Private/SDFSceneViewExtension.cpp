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

	// SceneViewExtension functions get called for ANY viewport
	// This includes preview scenes that might not include an associated world
	if (const UWorld* World{ InViewFamily.Scene->GetWorld() })
	{
		// We call this from here so it runs on the Game Thread
		TransferSDFsToRenderThread( InViewFamily.Scene->GetWorld() );
	}
};

void FSDFSceneViewExtension::TransferSDFsToRenderThread( const UWorld* World )
{
	// Only update if dirty
	// First check if the world we're rendering includes the SDF subsystem
	USDFSubsystem* SDFSubsystem{ World->GetSubsystem<USDFSubsystem>() };
	if (not SDFSubsystem || not SDFSubsystem->GetAndResetDirty()) { return; }

	// TODO: remove before PR (also other debug messages)
	// Debug
	GEngine->AddOnScreenDebugMessage( -1, // Force newline
									  0.2f, // Time
									  FColor::Yellow,
									  FString::Printf( TEXT( "Dirty SDF(s) found, updating render thread..." ) ) );

	// Grab the game thread's SDFs
	const auto& SDFs{ SDFSubsystem->GetRenderSDFs() };

	// Transfer lambda
	// Capture the parameters by copy, their state on the game thread might change before the lambda runs
	auto TransferParams{ [this, SDFs = SDFs]( FRHICommandList& ) mutable
	{
		// Copy the SDF data over to the render thread
		RT.SDFs.Empty();
		RT.SDFs.Reserve( SDFs.Num() );
		for (auto& Pair : SDFs)
		{
			RT.SDFs.Emplace( MoveTemp( Pair.Value ), Pair.Key );
		}
		RT.SDFResources.bDirty = true;
	} };

	// Perform the transfer safely on the render thread
	ENQUEUE_RENDER_COMMAND( SDFParamsThreadTransfer )( TransferParams );
}

void FSDFSceneViewExtension::PreRenderViewFamily_RenderThread( FRDGBuilder& GraphBuilder,
															   FSceneViewFamily& InViewFamily )
{
	UpdateSDFBuffer();
}

void FSDFSceneViewExtension::UpdateSDFBuffer()
{
	// The buffer is not up-to-date (or doesn't exist)
	if (not RT.SDFResources.bDirty) { return; }

	// We need to upload the updated SDF buffer
	RT.SDFResources.bUpload = true;

	// If the buffer hasn't been allocated or isn't large enough
	// If not, we simply overwrite the current data through the upload call
	if (not RT.SDFResources.Buff.IsValid()
		|| (RT.SDFResources.Buff->GetSize() / sizeof( FGPUSDF )) < static_cast<uint32>(RT.SDFs.Num()))
	{
		// We cannot allocate a buffer of size 0
		const uint32 Capacity{ static_cast<uint32>(FMath::Max( RT.SDFs.Num(), 1 )) };

		// We need a structured buffer (because we're storing an array of structs)
		// We need it to be an upload buffer, which are for CPU-to-GPU uploads
		const FRDGBufferDesc Desc{ FRDGBufferDesc::CreateStructuredUploadDesc( sizeof( FGPUSDF ), Capacity ) };

		// By manually allocating a buffer, we guarantee it's persistent among frames
		// This wouldn't be the case for 'GraphBuilder.CreateBuffer()'
		// If we already have a buffer allocated, it will be deallocated by the RDG when it goes out of scope
		RT.SDFResources.Buff = AllocatePooledBuffer( Desc, TEXT( "SDF Upload Buffer" ) );
	}
}

// PostRenderBasePassDeferred is called after rendering but before deferred lighting
void FSDFSceneViewExtension::PostRenderBasePassDeferred_RenderThread( FRDGBuilder& GraphBuilder,
																	  FSceneView& InView,
																	  const FRenderTargetBindingSlots& RenderTargets,
																	  TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures )
{
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
		const FRDGBufferRef SDFBuffRDG{ GraphBuilder.RegisterExternalBuffer( RT.SDFResources.Buff, ERDGBufferFlags::MultiFrame ) };
		if (RT.SDFResources.bUpload)
		{
			// 'NoCopy' indicates the RDG doesn't need to create its own copy of the specified upload data
			// We may only use it if we guarantee the provided data survives until the RDG uploads it
			GraphBuilder.QueueBufferUpload( SDFBuffRDG, 
											RT.SDFs.GetData(), 
											sizeof( FGPUSDF ) * RT.SDFs.Num(), 
											ERDGInitialDataFlags::NoCopy );
			RT.SDFResources.bUpload = false;
		}

		// Set shader bindings
		FSDFTraverseShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FSDFTraverseShader::FParameters>();
		PassParameters->View = InView.ViewUniformBuffer;
		PassParameters->SDFEdits = GraphBuilder.CreateSRV( SDFBuffRDG ); // Create the SDF buffer view (which specifies how the shader should read it)
		PassParameters->NumEdits = RT.SDFs.Num();
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