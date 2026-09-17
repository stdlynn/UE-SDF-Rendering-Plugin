// Custom SceneViewExtension Template for Unreal Engine
// Copyright 2023 - 2025 Ossi Luoto

// Plugin
#include "SDFExtension.h"
#include "SDFSettings.h"

// UE
#include "EngineUtils.h"
#include "RenderGraphUtils.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "PostProcess/SceneRenderTargets.h"

IMPLEMENT_GLOBAL_SHADER(FSDFTraverseShader, "/Plugins/SDFPlugin/TraversePS.usf", "TraversePS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FBuildBrickmapShader, "/Plugins/SDFPlugin/BuildBrickmapCS.usf", "BuildBrickmapCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FDebugShader, "/Plugins/SDFPlugin/DebugCS.usf", "DebugCS", SF_Compute);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FPtrBuffer, "PtrGridBuff");
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FBrickAllocCounter, "BrickAllocCounterBuff");

static const FIntVector AtlasSize = FIntVector(512, 512, 8);
static const FIntVector PtrGridSize = FIntVector(64, 64, 64);

namespace
{
	TAutoConsoleVariable<int32> CVarShaderOn(
		TEXT("r.SDF.Plugin"),
		1,
		TEXT("Enable SDF Plugin \n")
		TEXT(" 0: OFF;")
		TEXT(" 1: ON."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarDebugGrid(
		TEXT("r.SDF.Debug.Grid"),
		0,
		TEXT("Enable debug view of the pointer grid \n")
		TEXT(" 0: OFF;")
		TEXT(" 1: ON."),
		ECVF_RenderThreadSafe);
}

FSDFExtension::FSDFExtension(const FAutoRegister& AutoRegister, UWorld* InWorld) 
	: FSceneViewExtensionBase(AutoRegister), GT{ .World{ InWorld} }
{
	UE_LOG(LogTemp, Log, TEXT("SDFPlugin: Custom SceneViewExtension registered"));
}

void FSDFExtension::ReleaseResources()
{
	RT.BrickmapResources.Release();
}

bool FSDFExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	const UWorld* W = GT.World.Get();
	
	// Check if world exists
	if (!W) return false;
	// Check if the context has a scene that matches the world
	if (Context.Scene) return (Context.Scene == W->Scene);
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
};

void FSDFExtension::TransferSettingsToRenderThread()
{
	// Check if any of the settings are dirty
	const USDFSettings* SDFSettings{ GetDefault<USDFSettings>() };
	if (GT.Settings.PtrGridPos == SDFSettings->PtrGridPos 
		 && GT.Settings.VoxSize == SDFSettings->VoxSize
		 && GT.Settings.GPUUnitScale == SDFSettings->GPUUnitScale)
	{
		return;
	}

	GT.Settings.PtrGridPos = SDFSettings->PtrGridPos;
	GT.Settings.VoxSize = SDFSettings->VoxSize;
	GT.Settings.GPUUnitScale = SDFSettings->GPUUnitScale;

	// Transfer lambda
	// Capture the parameters by copy, their state on the game thread might change before the lambda runs
	auto TransferSettings{ [this, Settings = GT.Settings]( FRHICommandList& ) mutable
	{
		// Copy the settings over to the render thread
		// Convert to GPU unit scale
		RT.Settings.PtrGridPos = Settings.PtrGridPos * Settings.GPUUnitScale;
		RT.Settings.VoxSize = Settings.VoxSize * Settings.GPUUnitScale;
		RT.Settings.GPUUnitScale = Settings.GPUUnitScale;
	} };

	// Perform the transfer safely on the render thread
	ENQUEUE_RENDER_COMMAND( SDFSettingsThreadTransfer )( TransferSettings );
}

void FSDFExtension::TransferSDFsToRenderThread()
{
	// Only update if dirty
	USDFSubsystem* SDFSubsystem{ GT.World->GetSubsystem<USDFSubsystem>() };
	if (not SDFSubsystem->GetAndResetDirty()) { return; }

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

void FSDFExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	if (CVarShaderOn.GetValueOnRenderThread() == 0)
	{
		return;
	}

	UpdateSDFBuffer();

	if (InViewFamily.FrameCounter == RT.LastBrickmapBuildFrame)
	{
		return; // Already built brickmap this frame
	}
	RT.LastBrickmapBuildFrame = InViewFamily.FrameCounter;

	if (!RT.BrickmapResources.IsValid())
	{
		AllocateBrickmap();
	}

	// For now, just build it once and never again
	RDG_EVENT_SCOPE(GraphBuilder, "SDF_Build_Brickmap");
	{
		const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(InViewFamily.GetFeatureLevel());

		const FRDGTextureRef BrickAtlasRDG = GraphBuilder.RegisterExternalTexture(
			RT.BrickmapResources.BrickAtlas, ERDGTextureFlags::MultiFrame);
		const FRDGBufferRef PtrBufferRDG = GraphBuilder.RegisterExternalBuffer(
			RT.BrickmapResources.PtrGrid, ERDGBufferFlags::MultiFrame);
		const FRDGBufferRef AtomicCounterRDG = GraphBuilder.RegisterExternalBuffer(
			RT.BrickmapResources.BrickAllocCounter, ERDGBufferFlags::MultiFrame);

		const FRDGBufferRef SDFBuffRDG{ RegisterSDFBuffer( GraphBuilder ) };

		// Clear the brickmap every frame for now
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BrickAtlasRDG), 1.0f);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(PtrBufferRDG, PF_R32_UINT), 0xFFFFFFFFu);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(AtomicCounterRDG, PF_R32_UINT), 0);

		FBuildBrickmapShader::FParameters* PassParameters =
			GraphBuilder.AllocParameters<FBuildBrickmapShader::FParameters>();
		PassParameters->SDFEdits = GraphBuilder.CreateSRV(SDFBuffRDG); // Create the SDF buffer view (which specifies how the shader should read it)
		PassParameters->NumEdits = RT.SDFs.Num();
		PassParameters->BrickAtlas = GraphBuilder.CreateUAV(BrickAtlasRDG);
		PassParameters->PtrGrid = GraphBuilder.CreateUAV(PtrBufferRDG, PF_R32_UINT);
		PassParameters->BrickAllocCounter = GraphBuilder.CreateUAV(AtomicCounterRDG, PF_R32_UINT);
		PassParameters->PtrGridPos = RT.Settings.PtrGridPos;
		PassParameters->VoxSize = RT.Settings.VoxSize;
		PassParameters->UnitScale = RT.Settings.GPUUnitScale;
		PassParameters->GridSize = PtrGridSize;
		PassParameters->AtlasSize = AtlasSize;

		const FIntVector GroupCount{
			FMath::DivideAndRoundUp(1, 8),
			FMath::DivideAndRoundUp(1, 8),
			FMath::DivideAndRoundUp(1, 8)
		};

		const TShaderMapRef<FBuildBrickmapShader> ComputeShader(GlobalShaderMap);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Brickmap_Build"),
			ComputeShader,
			PassParameters,
			GroupCount);

		UE_LOG(LogTemp, Log, TEXT("SDFPlugin: Built Brickmap"));
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

FRDGBufferRef FSDFExtension::RegisterSDFBuffer(FRDGBuilder& GraphBuilder)
{
	const FRDGBufferRef SDFBuffRDG{ GraphBuilder.RegisterExternalBuffer(RT.SDFResources.Buff, ERDGBufferFlags::MultiFrame) };
	if (RT.SDFResources.bUpload)
	{
		// 'NoCopy' indicates the RDG doesn't need to create its own copy of the specified upload data
		// We may only use it if we guarantee the provided data survives until the RDG uploads it
		GraphBuilder.QueueBufferUpload(SDFBuffRDG,
			RT.SDFs.GetData(),
			sizeof(FGPUSDF) * RT.SDFs.Num(),
			ERDGInitialDataFlags::NoCopy);
		RT.SDFResources.bUpload = false;
	}
	return SDFBuffRDG;
}

void FSDFExtension::AllocateBrickmap()
{
	check(IsInRenderingThread());

	const FRDGTextureDesc BrickAtlasDesc = FRDGTextureDesc::Create3D(
		AtlasSize, PF_R8, FClearValueBinding::None,
		TexCreate_ShaderResource | TexCreate_UAV);
	RT.BrickmapResources.BrickAtlas = 
		AllocatePooledTexture(BrickAtlasDesc, TEXT("SDF_BrickAtlas"));

	const uint32 GridSize = PtrGridSize.X * PtrGridSize.Y * PtrGridSize.Z;
	const FRDGBufferDesc PtrGridDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), GridSize);
	RT.BrickmapResources.PtrGrid = 
		AllocatePooledBuffer(PtrGridDesc, TEXT("SDF_BrickPtrBuffer"));
	
	const FRDGBufferDesc CounterDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1);
	RT.BrickmapResources.BrickAllocCounter =
		AllocatePooledBuffer(CounterDesc, TEXT("SDF_BrickAllocCounter"));

	UE_LOG(LogTemp, Log, TEXT("SDFPlugin: Allocated Brickmap"));
	GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, TEXT("Allocated Brickmap"));
}

void FSDFExtension::PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
	if (CVarShaderOn.GetValueOnRenderThread() == 0)
	{
		return;
	}

	const FRDGTextureRef DepthTexture = RenderTargets.DepthStencil.GetTexture();
	if (!DepthTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("No Depth Stencil texture available for Rendering"));
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, TEXT("No Depth Stencil texture available for Rendering"));
		return;
	}

	if (!RT.BrickmapResources.IsValid())
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Brickmap Resources aren't valid!"));
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "SDF_Trace");
	{
		// Gives us more data from the FSceneView
		const FViewInfo& View = static_cast<const FViewInfo&>(InView);

		// Tell the RDG that the SDF buffer will be used this frame
		// Specify that it is persistent/external ('MultiFrame')
		const FRDGBufferRef SDFBuffRDG{ RegisterSDFBuffer( GraphBuilder ) };

		const FRDGTextureRef BrickAtlasRDG = GraphBuilder.RegisterExternalTexture(
			RT.BrickmapResources.BrickAtlas, ERDGTextureFlags::MultiFrame);
		const FRDGBufferRef PtrBufferRDG = GraphBuilder.RegisterExternalBuffer(
			RT.BrickmapResources.PtrGrid, ERDGBufferFlags::MultiFrame);
		FRHISamplerState* BrickSampler = 
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		FSDFTraverseShader::FParameters* PassParameters =
			GraphBuilder.AllocParameters<FSDFTraverseShader::FParameters>();
		PassParameters->View = InView.ViewUniformBuffer;
		PassParameters->SDFEdits = GraphBuilder.CreateSRV( SDFBuffRDG ); // Create the SDF buffer view (which specifies how the shader should read it)
		PassParameters->NumEdits = RT.SDFs.Num();
		PassParameters->BrickAtlas = GraphBuilder.CreateSRV(BrickAtlasRDG);
		PassParameters->BrickSampler = BrickSampler;
		PassParameters->PtrGrid = GraphBuilder.CreateSRV(PtrBufferRDG, PF_R32_UINT);
		PassParameters->PtrGridPos = RT.Settings.PtrGridPos;
		PassParameters->VoxSize = RT.Settings.VoxSize;
		PassParameters->UnitScale = RT.Settings.GPUUnitScale;
		PassParameters->RenderTargets = RenderTargets;
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
			DepthTexture,
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
		static constexpr uint32 StencilRef =
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

void FSDFExtension::SubscribeToPostProcessingPass(
	EPostProcessingPass PassId, const FSceneView& InView, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	if (PassId == EPostProcessingPass::MotionBlur)
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FSDFExtension::RenderDebug));
	}
}

FScreenPassTexture FSDFExtension::RenderDebug(FRDGBuilder& GraphBuilder, const FSceneView& SceneView, const FPostProcessMaterialInputs& Inputs)
{
	const FSceneViewFamily& ViewFamily = *SceneView.Family;

	const FScreenPassTexture& SceneColor = FScreenPassTexture::CopyFromSlice(GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SceneColor));

	if (!SceneColor.IsValid() || CVarDebugGrid.GetValueOnRenderThread() == 0)
	{
		return SceneColor;
	}

	const FScreenPassTextureViewport SceneColorViewport(SceneColor);

	RDG_EVENT_SCOPE(GraphBuilder, "SDF_Debug");
	{
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(ViewFamily.GetFeatureLevel());

		FRDGTextureDesc OutputDesc;
		{
			OutputDesc = SceneColor.Texture->Desc;

			OutputDesc.Reset();
			OutputDesc.Flags |= TexCreate_UAV;
			OutputDesc.Flags &= ~(TexCreate_RenderTargetable | TexCreate_FastVRAM);

			FLinearColor ClearColor(0., 0., 0., 0.);
			OutputDesc.ClearValue = FClearValueBinding(ClearColor);
		}

		FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("Custom Effect Output Texture"));

		FRDGBufferRef PtrBufferRDG = GraphBuilder.RegisterExternalBuffer(
			RT.BrickmapResources.PtrGrid, ERDGBufferFlags::MultiFrame);

		FDebugShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FDebugShader::FParameters>();
		PassParameters->OriginalSceneColor = SceneColor.Texture;
		PassParameters->DepthTex = Inputs.SceneTextures.SceneTextures->GetParameters()->SceneDepthTexture;
		PassParameters->View = SceneView.ViewUniformBuffer;
		PassParameters->PtrGrid = GraphBuilder.CreateSRV(PtrBufferRDG, PF_R32_UINT);
		PassParameters->SceneColorViewport = GetScreenPassTextureViewportParameters(SceneColorViewport);
		PassParameters->PtrGridPos = RT.Settings.PtrGridPos;
		PassParameters->VoxSize = RT.Settings.VoxSize;
		PassParameters->UnitScale = RT.Settings.GPUUnitScale;
		PassParameters->Output = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutputTexture));

		FIntPoint PassViewSize = SceneColor.ViewRect.Size();
		FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(PassViewSize, FComputeShaderUtils::kGolden2DGroupSize);

		TShaderMapRef<FDebugShader> ComputeShader(GlobalShaderMap);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SDF Debug Post Processing CS Shader %dx%d", PassViewSize.X, PassViewSize.Y),
			ComputeShader,
			PassParameters,
			GroupCount);

		AddCopyTexturePass(GraphBuilder, OutputTexture, SceneColor.Texture);
	}

	return SceneColor;
}
