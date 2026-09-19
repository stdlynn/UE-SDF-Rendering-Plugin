// Custom SceneViewExtension Template for Unreal Engine
// Copyright 2023 - 2025 Ossi Luoto

// Plugin
#include "SDFExtension.h"
#include "SDFSettings.h"
#include "BrickmapCommon.h"

// UE
#include "EngineUtils.h"
#include "RenderGraphUtils.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "PostProcess/SceneRenderTargets.h"

IMPLEMENT_GLOBAL_SHADER( FSDFTraverseShader, "/Plugins/SDFPlugin/TraversePS.usf", "TraversePS", SF_Pixel );
IMPLEMENT_GLOBAL_SHADER( FBuildBrickmapShader, "/Plugins/SDFPlugin/BuildBrickmapCS.usf", "BuildBrickmapCS", SF_Compute );
IMPLEMENT_GLOBAL_SHADER( FTransferDeallocatedShader, "/Plugins/SDFPlugin/BuildBrickmapCS.usf", "TransferDeallocatedCS", SF_Compute );
IMPLEMENT_GLOBAL_SHADER( FResetCountersShader, "/Plugins/SDFPlugin/BuildBrickmapCS.usf", "ResetCountersCS", SF_Compute );
IMPLEMENT_GLOBAL_SHADER( FDebugShader, "/Plugins/SDFPlugin/DebugCS.usf", "DebugCS", SF_Compute );

namespace
{
	// TODO: if this is turned on, we need to do a full rebuilt of the brickmap
	// TODO: since X frames of update lists have been skipped
	// TODO: Also note that this should be checked before running ANY shader
	TAutoConsoleVariable<int32> CVarShadersOn( TEXT( "r.SDF.Plugin" ),
											   1,
											   TEXT( "Enable SDF Plugin \n" )
											   TEXT( " 0: OFF;" )
											   TEXT( " 1: ON." ),
											   ECVF_RenderThreadSafe );

	TAutoConsoleVariable<int32> CVarRenderEdits( TEXT( "r.SDF.Edits" ),
												 1,
												 TEXT( "Enable SDF rendering \n" )
												 TEXT( " 0: OFF;" )
												 TEXT( " 1: ON." ),
												 ECVF_RenderThreadSafe );

	TAutoConsoleVariable<int32> CVarDebugGrid( TEXT( "r.SDF.Debug.Grid" ),
											   0,
											   TEXT( "Enable debug view of the pointer grid \n" )
											   TEXT( " 0: OFF;" )
											   TEXT( " 1: ON." ),
											   ECVF_RenderThreadSafe );
}

FSDFExtension::FSDFExtension( const FAutoRegister& AutoRegister, UWorld* InWorld )
	: FSceneViewExtensionBase( AutoRegister )
	, GT{ .World{ InWorld } }
{
	UE_LOG( LogTemp, Log, TEXT("SDFPlugin: Custom SceneViewExtension registered") );
}

void FSDFExtension::ReleaseResources()
{
	RT.BrickmapBuilder.ReleaseResources();
}

bool FSDFExtension::IsActiveThisFrame_Internal( const FSceneViewExtensionContext& Context ) const
{
	const UWorld* W = GT.World.Get();

	// Check if world exists
	if (! W) { return false; }
	// Check if the context has a scene that matches the world
	if (Context.Scene) { return (Context.Scene == W->Scene); }
	// Check if the context has a viewport that matches the world
	if (Context.Viewport)
	{
		if (const FViewportClient* Client = Context.Viewport->GetClient())
		{
			return Client->GetWorld() == W;
		}
	}

	return false;
}

void FSDFExtension::BeginRenderViewFamily( FSceneViewFamily& InViewFamily )
{
	// 'BeginRenderViewFamily' is called when the view family is about to be rendered
	// Allowing for game thread work before rendering a view family
	// A view represents a single viewpoint (camera) that we render from
	// A view family is a group of views that gets rendered together for the same frame/setup

	// We call this from here so it runs on the Game Thread
	TransferSettingsToRenderThread(); // This first! We use 'RT.Settings.UnitScale' for the rest
	TransferSDFsToRenderThread();
	GT.Settings.bUnitScaleDirty = false;
}

void FSDFExtension::TransferSettingsToRenderThread()
{
	// Check if any of the settings are dirty
	const USDFSettings* SDFSettings{ GetDefault<USDFSettings>() };
	if (GT.Settings.PtrGridPos == SDFSettings->PtrGridPos
		&& GT.Settings.VoxSize == SDFSettings->VoxSize
		&& GT.Settings.GPUUnitScale == SDFSettings->GPUUnitScale
		&& GT.Settings.DbgVisMaxBricks == SDFSettings->DbgVisMaxBricks)
	{
		return;
	}

	// Store settings to compare next frame
	GT.Settings.PtrGridPos = SDFSettings->PtrGridPos;
	GT.Settings.VoxSize = SDFSettings->VoxSize;
	GT.Settings.GPUUnitScale = SDFSettings->GPUUnitScale;
	GT.Settings.DbgVisMaxBricks = SDFSettings->DbgVisMaxBricks;
	GT.Settings.bUnitScaleDirty = true;

	// Transfer lambda
	// Capture the parameters by copy, their state on the game thread might change before the lambda runs
	auto TransferSettings{ [this, Settings = GT.Settings]( FRHICommandList& ) mutable
	{
		// Copy the settings over to the render thread
		RT.Settings = Settings;

		// Convert to GPU unit scale
		RT.Settings.PtrGridPos *= Settings.GPUUnitScale;
		RT.Settings.VoxSize *= Settings.GPUUnitScale;
	} };

	// Perform the transfer safely on the render thread
	ENQUEUE_RENDER_COMMAND( SDFSettingsThreadTransfer )( TransferSettings );
}

void FSDFExtension::TransferSDFsToRenderThread()
{
	// Only update if dirty
	USDFSubsystem* SDFSubsystem{ GT.World->GetSubsystem<USDFSubsystem>() };
	if (not SDFSubsystem->GetAndResetDirty() && not GT.Settings.bUnitScaleDirty) { return; }

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
		for (const auto& Pair : SDFs)
		{
			RT.SDFs.Emplace( Pair.Value, Pair.Key, RT.Settings.GPUUnitScale );
		}
		RT.SDFResources.bDirty = true;
	} };

	// Perform the transfer safely on the render thread
	ENQUEUE_RENDER_COMMAND( SDFParamsThreadTransfer )( TransferParams );
}

void FSDFExtension::PreRenderViewFamily_RenderThread( FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily )
{
	if (CVarShadersOn.GetValueOnRenderThread() == 0)
	{
		return;
	}

	if (InViewFamily.FrameCounter == RT.BrickmapBuilder.GetLastBuildFrame())
	{
		return; // Already built brickmap this frame
	}
	RT.BrickmapBuilder.SetLastBuildFrame( InViewFamily.FrameCounter );

	RT.BrickmapBuilder.SetBrickUpdateList( RT.SDFs, RT.Settings.PtrGridPos, RT.Settings.VoxSize );

	// Update upload buffers
	UpdateSDFBuffer();
	RT.BrickmapBuilder.AllocateBrickList();

	if (! RT.BrickmapBuilder.AreResourcesValid())
	{
		RT.BrickmapBuilder.AllocateResources();
	}

	RDG_EVENT_SCOPE( GraphBuilder, "SDF_Brickmap" );
	{
		const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap( InViewFamily.GetFeatureLevel() );

		const FRDGBufferRef BrickListRDG = RT.BrickmapBuilder.RegisterBrickListBuffer( GraphBuilder );
		const FRDGTextureRef BrickAtlasRDG = GraphBuilder.RegisterExternalTexture( RT.BrickmapBuilder.GetBrickAtlas(), ERDGTextureFlags::MultiFrame );
		const FRDGBufferRef PtrBufferRDG = GraphBuilder.RegisterExternalBuffer( RT.BrickmapBuilder.GetPtrGrid(), ERDGBufferFlags::MultiFrame );
		const FRDGBufferRef AllocCountersRDG = GraphBuilder.RegisterExternalBuffer( RT.BrickmapBuilder.GetBrickAllocCounter(), ERDGBufferFlags::MultiFrame );
		const FRDGBufferRef FreedListRDG = GraphBuilder.RegisterExternalBuffer( RT.BrickmapBuilder.GetFreedList(), ERDGBufferFlags::MultiFrame );
		const FRDGBufferRef DeallocatedListRDG = GraphBuilder.RegisterExternalBuffer( RT.BrickmapBuilder.GetDeallocatedList(), ERDGBufferFlags::MultiFrame );

		const FRDGBufferRef SDFBuffRDG{ RegisterSDFBuffer( GraphBuilder ) };
		const FRDGBufferUAVRef FreedListUAV = GraphBuilder.CreateUAV( FreedListRDG, PF_R32_UINT );
		const FRDGBufferUAVRef DeallocatedListUAV = GraphBuilder.CreateUAV( DeallocatedListRDG, PF_R32_UINT );
		const FRDGBufferUAVRef AllocCountersUAV = GraphBuilder.CreateUAV( AllocCountersRDG, PF_R32_UINT );

		if (RT.BrickmapBuilder.bReset)
		{
			AddClearUAVPass( GraphBuilder, GraphBuilder.CreateUAV( PtrBufferRDG, PF_R32_UINT ), 0xFFFFFFFFu );
			AddClearUAVPass( GraphBuilder, GraphBuilder.CreateUAV( BrickAtlasRDG ), 1.0f );
			AddClearUAVPass( GraphBuilder, FreedListUAV, 0xFFFFFFFFu );
			AddClearUAVPass( GraphBuilder, DeallocatedListUAV, 0xFFFFFFFFu );
			AddClearUAVPass( GraphBuilder, AllocCountersUAV, 0u );
			RT.BrickmapBuilder.bReset = false;
		}

		FBuildBrickmapShader::FParameters* ParametersBuild = GraphBuilder.AllocParameters<FBuildBrickmapShader::FParameters>();
		ParametersBuild->NumEdits = RT.SDFs.Num();
		ParametersBuild->SDFEdits = GraphBuilder.CreateSRV( SDFBuffRDG );
		ParametersBuild->PtrGrid = GraphBuilder.CreateUAV( PtrBufferRDG, PF_R32_UINT );
		ParametersBuild->BrickAtlas = GraphBuilder.CreateUAV( BrickAtlasRDG );
		ParametersBuild->GridSize = FIntVector3( PtrGridSize );
		ParametersBuild->AtlasSize = FIntVector3( AtlasSize );
		ParametersBuild->PtrGridPos = RT.Settings.PtrGridPos;
		ParametersBuild->VoxSize = RT.Settings.VoxSize;
		ParametersBuild->UnitScale = RT.Settings.GPUUnitScale;
		ParametersBuild->NumUpdated = RT.BrickmapBuilder.GetNumUpdatedBricks();
		ParametersBuild->UpdateList = GraphBuilder.CreateSRV( BrickListRDG, PF_R32_UINT );
		ParametersBuild->FreedList = FreedListUAV;
		ParametersBuild->DeallocatedList = DeallocatedListUAV;
		ParametersBuild->BrickAllocCounters = AllocCountersUAV;

		FTransferDeallocatedShader::FParameters* ParametersTransfer = GraphBuilder.AllocParameters<FTransferDeallocatedShader::FParameters>();
		ParametersTransfer->FreedList = FreedListUAV;
		ParametersTransfer->DeallocatedList = DeallocatedListUAV;
		ParametersTransfer->BrickAllocCounters = AllocCountersUAV;

		FResetCountersShader::FParameters* ParametersReset = GraphBuilder.AllocParameters<FResetCountersShader::FParameters>();
		ParametersReset->FreedList = FreedListUAV;
		ParametersReset->DeallocatedList = DeallocatedListUAV;
		ParametersReset->BrickAllocCounters = AllocCountersUAV;

		const FIntVector GroupCountBuild
		{
			FMath::DivideAndRoundUp( static_cast<int>(ParametersBuild->NumUpdated * BrickResolution), 8 ),
			FMath::DivideAndRoundUp( static_cast<int>(BrickResolution), 8 ),
			FMath::DivideAndRoundUp( static_cast<int>(BrickResolution), 8 )
		};
		const FIntVector GroupCountTransfer
		{
			FMath::DivideAndRoundUp( static_cast<int>(RT.BrickmapBuilder.GetNumUpdatedBricks()), 64 ), 1, 1
		};
		const FIntVector GroupCountReset{ 1, 1, 1 };

		const TShaderMapRef<FBuildBrickmapShader> ComputeBuild( GlobalShaderMap );
		const TShaderMapRef<FTransferDeallocatedShader> ComputeTransfer( GlobalShaderMap );
		const TShaderMapRef<FResetCountersShader> ComputeReset( GlobalShaderMap );

		FComputeShaderUtils::AddPass( GraphBuilder,
									  RDG_EVENT_NAME( "Brickmap_Build" ),
									  ComputeBuild,
									  ParametersBuild,
									  GroupCountBuild );

		FComputeShaderUtils::AddPass( GraphBuilder,
									  RDG_EVENT_NAME( "Brickmap_Transfer" ),
									  ComputeTransfer,
									  ParametersTransfer,
									  GroupCountTransfer );

		FComputeShaderUtils::AddPass( GraphBuilder,
									  RDG_EVENT_NAME( "Brickmap_Reset" ),
									  ComputeReset,
									  ParametersReset,
									  GroupCountReset );
	}
}

void FSDFExtension::UpdateSDFBuffer()
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

FRDGBufferRef FSDFExtension::RegisterSDFBuffer( FRDGBuilder& GraphBuilder )
{
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
	return SDFBuffRDG;
}

void FSDFExtension::PostRenderBasePassDeferred_RenderThread( FRDGBuilder& GraphBuilder,
															 FSceneView& InView,
															 const FRenderTargetBindingSlots& RenderTargets,
															 TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures )
{
	if (CVarShadersOn.GetValueOnRenderThread() == 0
		|| CVarRenderEdits.GetValueOnRenderThread() == 0)
	{
		return;
	}

	const FRDGTextureRef DepthTexture = RenderTargets.DepthStencil.GetTexture();
	if (! DepthTexture)
	{
		UE_LOG( LogTemp, Error, TEXT("No Depth Stencil texture available for Rendering") );
		GEngine->AddOnScreenDebugMessage( -1, 15.0f, FColor::Red, TEXT( "No Depth Stencil texture available for Rendering" ) );
		return;
	}

	if (! RT.BrickmapBuilder.AreResourcesValid())
	{
		GEngine->AddOnScreenDebugMessage( -1, 5.0f, FColor::Red, TEXT( "Brickmap Resources aren't valid!" ) );
		return;
	}

	RDG_EVENT_SCOPE( GraphBuilder, "SDF_Trace" );
	{
		// Gives us more data from the FSceneView
		const FViewInfo& View = static_cast<const FViewInfo&>(InView);

		// Tell the RDG that the SDF buffer will be used this frame
		// Specify that it is persistent/external ('MultiFrame')
		const FRDGBufferRef SDFBuffRDG{ RegisterSDFBuffer( GraphBuilder ) };

		const FRDGTextureRef BrickAtlasRDG = GraphBuilder.RegisterExternalTexture( RT.BrickmapBuilder.GetBrickAtlas(), ERDGTextureFlags::MultiFrame );
		const FRDGBufferRef PtrBufferRDG = GraphBuilder.RegisterExternalBuffer( RT.BrickmapBuilder.GetPtrGrid(), ERDGBufferFlags::MultiFrame );
		FRHISamplerState* BrickSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		FSDFTraverseShader::FParameters* PassParameters =
				GraphBuilder.AllocParameters<FSDFTraverseShader::FParameters>();
		PassParameters->View = InView.ViewUniformBuffer;
		PassParameters->SDFEdits = GraphBuilder.CreateSRV( SDFBuffRDG ); // Create the SDF buffer view (which specifies how the shader should read it)
		PassParameters->NumEdits = RT.SDFs.Num();
		PassParameters->BrickAtlas = GraphBuilder.CreateSRV( BrickAtlasRDG );
		PassParameters->BrickSampler = BrickSampler;
		PassParameters->PtrGrid = GraphBuilder.CreateSRV( PtrBufferRDG, PF_R32_UINT );
		PassParameters->PtrGridPos = RT.Settings.PtrGridPos;
		PassParameters->VoxSize = RT.Settings.VoxSize;
		PassParameters->UnitScale = RT.Settings.GPUUnitScale;
		PassParameters->RenderTargets = RenderTargets;
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding( DepthTexture,
																		   ERenderTargetLoadAction::ELoad, // Depth
																		   ERenderTargetLoadAction::ELoad, // Stencil
																		   FExclusiveDepthStencil::DepthWrite_StencilWrite );

		FRHIDepthStencilState* DepthStencilState = TStaticDepthStencilState<true, // bEnableDepthTest
																			CF_DepthNearOrEqual,
																			true, // bEnableFrontFaceStencil
																			CF_Always, SO_Keep, SO_Keep, SO_Replace,
																			false, // bEnableBackFaceStencil
																			CF_Always, SO_Keep, SO_Keep, SO_Keep,
																			0x0, // StencilReadMask
																			0x0>::GetRHI();

		const TShaderMapRef<FSDFTraverseShader> PixelShader( View.ShaderMap );

		// For if we ever need to write to the stencil buffer
		static constexpr uint32 StencilRef =
				GET_STENCIL_BIT_MASK( RECEIVE_DECAL, 1 ) |
				STENCIL_LIGHTING_CHANNELS_MASK( 0x1 );

		FPixelShaderUtils::AddFullscreenPass( GraphBuilder,
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

void FSDFExtension::SubscribeToPostProcessingPass(
	EPostProcessingPass PassId, const FSceneView& InView, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled )
{
	if (PassId == EPostProcessingPass::MotionBlur)
	{
		InOutPassCallbacks.Add( FAfterPassCallbackDelegate::CreateRaw( this, &FSDFExtension::RenderDebug ) );
	}
}

FScreenPassTexture FSDFExtension::RenderDebug( FRDGBuilder& GraphBuilder, const FSceneView& SceneView, const FPostProcessMaterialInputs& Inputs )
{
	const FSceneViewFamily& ViewFamily = *SceneView.Family;
	const FScreenPassTexture& SceneColor = FScreenPassTexture::CopyFromSlice( GraphBuilder, Inputs.GetInput( EPostProcessMaterialInput::SceneColor ) );

	if (! SceneColor.IsValid()
		|| CVarShadersOn.GetValueOnRenderThread() == 0
		|| CVarDebugGrid.GetValueOnRenderThread() == 0)
	{
		return SceneColor;
	}

	const FScreenPassTextureViewport SceneColorViewport( SceneColor );

	RDG_EVENT_SCOPE( GraphBuilder, "SDF_Debug" );
	{
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap( ViewFamily.GetFeatureLevel() );

		FRDGTextureDesc OutputDesc;
		{
			OutputDesc = SceneColor.Texture->Desc;

			OutputDesc.Reset();
			OutputDesc.Flags |= TexCreate_UAV;
			OutputDesc.Flags &= ~(TexCreate_RenderTargetable | TexCreate_FastVRAM);

			FLinearColor ClearColor( 0., 0., 0., 0. );
			OutputDesc.ClearValue = FClearValueBinding( ClearColor );
		}

		FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture( OutputDesc, TEXT( "Custom Effect Output Texture" ) );
		FRDGBufferRef PtrBufferRDG = GraphBuilder.RegisterExternalBuffer( RT.BrickmapBuilder.GetPtrGrid(), ERDGBufferFlags::MultiFrame );

		FDebugShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FDebugShader::FParameters>();
		PassParameters->OriginalSceneColor = SceneColor.Texture;
		PassParameters->DepthTex = Inputs.SceneTextures.SceneTextures->GetParameters()->SceneDepthTexture;
		PassParameters->View = SceneView.ViewUniformBuffer;
		PassParameters->PtrGrid = GraphBuilder.CreateSRV( PtrBufferRDG, PF_R32_UINT );
		PassParameters->SceneColorViewport = GetScreenPassTextureViewportParameters( SceneColorViewport );
		PassParameters->PtrGridPos = RT.Settings.PtrGridPos;
		PassParameters->VoxSize = RT.Settings.VoxSize;
		PassParameters->UnitScale = RT.Settings.GPUUnitScale;
		PassParameters->DbgVisMaxBricks = RT.Settings.DbgVisMaxBricks;
		PassParameters->Output = GraphBuilder.CreateUAV( FRDGTextureUAVDesc( OutputTexture ) );

		FIntPoint PassViewSize = SceneColor.ViewRect.Size();
		FIntVector GroupCount = FComputeShaderUtils::GetGroupCount( PassViewSize, FComputeShaderUtils::kGolden2DGroupSize );

		TShaderMapRef<FDebugShader> ComputeShader( GlobalShaderMap );

		FComputeShaderUtils::AddPass( GraphBuilder,
									  RDG_EVENT_NAME( "SDF Debug Post Processing CS Shader %dx%d", PassViewSize.X, PassViewSize.Y ),
									  ComputeShader,
									  PassParameters,
									  GroupCount );

		AddCopyTexturePass( GraphBuilder, OutputTexture, SceneColor.Texture );
	}

	return SceneColor;
}