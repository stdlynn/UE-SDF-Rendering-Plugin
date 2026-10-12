// Custom SceneViewExtension Template for Unreal Engine
// Copyright 2023 - 2025 Ossi Luoto
// 
// Main Module to set Shader Directories

// Plugin
#include "SDFPlugin.h"
#include "SDFExtension.h"

// UE
#include "Interfaces/IPluginManager.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CoreDelegates.h"
#include "Engine/Engine.h"

#define LOCTEXT_NAMESPACE "SDFPlugin"

#if !UE_BUILD_SHIPPING
class FSDFDebugInputProcessor : public IInputProcessor
{
public:
    virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}

    virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
    {
        if (Event.GetKey() == EKeys::F6 && Event.IsControlDown() && Event.IsControlDown() && !Event.IsRepeat())
        {
            UWorld* World = nullptr;
            for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
            {
                if (Ctx.WorldType == EWorldType::PIE) { World = Ctx.World(); break; }
                if (Ctx.WorldType == EWorldType::Editor || Ctx.WorldType == EWorldType::Game) { World = Ctx.World(); }
            }
            GEngine->Exec(World, TEXT("r.SDF.Debug.Clear"));
            return true;
        }
        return false;
    }
};
#endif

void FSDFPlugin::StartupModule()
{
	// Set up the Shader Directories
	const FString BaseDir = IPluginManager::Get().FindPlugin( TEXT( "SDFPlugin" ) )->GetBaseDir();
	const FString PluginShaderDir = FPaths::Combine( BaseDir, TEXT( "Shaders" ) );
	AddShaderSourceDirectoryMapping( TEXT( "/Plugins/SDFPlugin" ), PluginShaderDir );

#if !UE_BUILD_SHIPPING
    FCoreDelegates::GetOnPostEngineInit().AddLambda([]()
        {
            if (FSlateApplication::IsInitialized())
            {
                FSlateApplication::Get().RegisterInputPreProcessor(MakeShared<FSDFDebugInputProcessor>());
            }
        });
#endif
    }

void FSDFPlugin::ShutdownModule() {}

ASDFActor* USDFPluginLibrary::SpawnSDF( const UObject* WorldContextObject, const FVector& Location, const FSDFEdit& Settings )
{
    UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject, EGetWorldErrorMode::LogAndReturnNull );
	if (!World) return nullptr;

    ASDFActor* Actor = World->SpawnActor<ASDFActor>( Location, FRotator::ZeroRotator );
    USDFSubsystem* Subsystem = World->GetSubsystem<USDFSubsystem>();
    if (Subsystem && Actor)
    {
        USDFComponent* Comp = Actor->GetSDFComponent();

		Comp->SetAllEditProperties( Settings );
        Comp->SetWorldLocation( Location );
        Subsystem->RegisterSDF( *Comp );
        return Actor;
    }

    return nullptr;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE( FSDFPlugin, SDFPlugin );