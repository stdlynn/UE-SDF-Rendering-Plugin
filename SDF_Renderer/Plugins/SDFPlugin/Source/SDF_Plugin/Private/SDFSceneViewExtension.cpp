// Custom SceneViewExtension Template for Unreal Engine
// Copyright 2023 - 2025 Ossi Luoto

#include "SDFSceneViewExtension.h"
#include "PostProcess/SceneRenderTargets.h"
#include "RenderGraphUtils.h"

IMPLEMENT_GLOBAL_SHADER(FSDFTraverseShader, "/Plugins/SDFPlugin/PostProcessCS.usf", "TraversePS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FBuildBrickmapShader, "/Plugins/SDFPlugin/BuildBrickmapCS.usf", "BuildBrickmapCS", SF_Compute);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FPtrBuffer, "PtrGrid");
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FBrickAllocCounter, "BrickAllocCounter");

static const FIntVector AtlasSize = FIntVector(512, 512, 8);
static const FIntVector PtrGridSize = FIntVector(64, 64, 64);

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

FSDFSceneViewExtension::FSDFSceneViewExtension(const FAutoRegister& AutoRegister, UWorld* InWorld) 
	: FSceneViewExtensionBase(AutoRegister), World(InWorld)
{
	UE_LOG(LogTemp, Log, TEXT("SDFPlugin: Custom SceneViewExtension registered"));
}

bool FSDFSceneViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	const UWorld* W = World.Get();
	
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

void FSDFSceneViewExtension::AllocateBrickmap()
{
	check(IsInRenderingThread());

	FRDGTextureDesc BrickAtlasDesc = FRDGTextureDesc::Create3D(
		AtlasSize, PF_R8, FClearValueBinding::None,
		TexCreate_ShaderResource | TexCreate_UAV);
	BrickmapResources.BrickAtlas = 
		AllocatePooledTexture(BrickAtlasDesc, TEXT("SDF_BrickAtlas"));

	const uint32 GridSize = PtrGridSize.X * PtrGridSize.Y * PtrGridSize.Z;
	FRDGBufferDesc PtrGridDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), GridSize);
	BrickmapResources.PtrGrid = 
		AllocatePooledBuffer(PtrGridDesc, TEXT("SDF_BrickPtrBuffer"));
	
	FRDGBufferDesc CounterDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1);
	BrickmapResources.BrickAllocCounter =
		AllocatePooledBuffer(CounterDesc, TEXT("SDF_BrickAllocCounter"));

	UE_LOG(LogTemp, Log, TEXT("SDFPlugin: Allocated Brickmap"));
	GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, TEXT("Allocated Brickmap"));
}

void FSDFSceneViewExtension::ReleaseResources()
{
	BrickmapResources.Release();
}

void FSDFSceneViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	if (InViewFamily.FrameCounter == LastBrickmapBuildFrame)
	{
		return; // Already built brickmap this frame
	}
	LastBrickmapBuildFrame = InViewFamily.FrameCounter;

	if (!BrickmapResources.IsValid())
	{
		AllocateBrickmap();
		
		// For now, just build it once and never again
		RDG_EVENT_SCOPE(GraphBuilder, "Build Brickmap");
		{
			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(InViewFamily.GetFeatureLevel());

			FRDGTextureRef BrickAtlasRDG = GraphBuilder.RegisterExternalTexture(
				BrickmapResources.BrickAtlas, ERDGTextureFlags::MultiFrame);
			FRDGBufferRef PtrBufferRDG = GraphBuilder.RegisterExternalBuffer(
				BrickmapResources.PtrGrid, ERDGBufferFlags::MultiFrame);
			FRDGBufferRef AtomicCounterRDG = GraphBuilder.RegisterExternalBuffer(
				BrickmapResources.BrickAllocCounter, ERDGBufferFlags::MultiFrame);

			const FRDGTextureDesc BrickAtlasDesc = FRDGTextureDesc::Create3D(
				AtlasSize, PF_R8, FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_UAV);

			// Clear the brickmap every frame for now
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(PtrBufferRDG), 0xFFFFFFFFu);

			FBuildBrickmapShader::FParameters* PassParameters =
				GraphBuilder.AllocParameters<FBuildBrickmapShader::FParameters>();
			PassParameters->BrickAtlas = GraphBuilder.CreateUAV(BrickAtlasRDG);
			PassParameters->PtrGrid = GraphBuilder.CreateUAV(PtrBufferRDG);
			PassParameters->BrickAllocCounter = GraphBuilder.CreateUAV(AtomicCounterRDG);
			PassParameters->GridSize = PtrGridSize;
			PassParameters->AtlasSize = AtlasSize;

			FIntVector GroupCount{
				FMath::DivideAndRoundUp(1, 8),
				FMath::DivideAndRoundUp(1, 8),
				FMath::DivideAndRoundUp(1, 8)
			};

			TShaderMapRef<FBuildBrickmapShader> ComputeShader(GlobalShaderMap);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Brickmap_Build"),
				ComputeShader,
				PassParameters,
				GroupCount);
		}
	}

}

void FSDFSceneViewExtension::PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
	FRDGTextureRef DepthTexture = RenderTargets.DepthStencil.GetTexture();
	if (!DepthTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("No Depth Stencil texture available for Rendering"));
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, TEXT("No Depth Stencil texture available for Rendering"));
		return;
	}

	if (!BrickmapResources.IsValid())
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Brickmap Resources aren't valid!"));
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "SDF_Trace");
	{
		// Gives us more data from the FSceneView
		const FViewInfo& View = static_cast<const FViewInfo&>(InView);

		FRDGTextureRef BrickAtlasRDG = GraphBuilder.RegisterExternalTexture(
			BrickmapResources.BrickAtlas, ERDGTextureFlags::MultiFrame);
		FRDGBufferRef PtrBufferRDG = GraphBuilder.RegisterExternalBuffer(
			BrickmapResources.PtrGrid, ERDGBufferFlags::MultiFrame);
		FRHISamplerState* BrickSampler = 
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		FSDFTraverseShader::FParameters* PassParameters =
			GraphBuilder.AllocParameters<FSDFTraverseShader::FParameters>();
		PassParameters->View = InView.ViewUniformBuffer;
		PassParameters->BrickAtlas = GraphBuilder.CreateSRV(BrickAtlasRDG);
		PassParameters->BrickSampler = BrickSampler;
		PassParameters->PtrBuffer = GraphBuilder.CreateSRV(PtrBufferRDG);
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

		TShaderMapRef<FSDFTraverseShader> PixelShader(View.ShaderMap);

		// For if we ever need to write to the stencil buffer
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
			TStaticBlendState<>::GetRHI(), // Default blend state
			TStaticRasterizerState<FM_Solid, CM_None>::GetRHI(), // Fill mode, Cull Mode
			DepthStencilState,
			StencilRef);
	}
}