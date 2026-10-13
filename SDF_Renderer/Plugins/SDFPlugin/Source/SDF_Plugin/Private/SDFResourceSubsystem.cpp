#include "SDFResourceSubsystem.h"
#include "SDFExtension.h"
#include "SceneViewExtension.h"
#include "HAL/IConsoleManager.h"

static FAutoConsoleCommandWithWorld GSDFDebugClearCmd(
	TEXT("r.SDF.Debug.Clear"),
	TEXT("Clears the entire brickmap to remove stale cache"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](const UWorld* World)
		{
			if (!World) return;
			if (USDFResourceSubsystem* Subsystem = World->GetSubsystem<USDFResourceSubsystem>())
			{
				Subsystem->ResetSDF();
			}
		}),
	ECVF_Cheat);

bool USDFResourceSubsystem::ShouldCreateSubsystem( UObject* Outer ) const
{
	if (! Super::ShouldCreateSubsystem( Outer ))
	{
		return false;
	}

	return FApp::CanEverRender();
}

bool USDFResourceSubsystem::DoesSupportWorldType( const EWorldType::Type WorldType ) const
{
	return WorldType == EWorldType::Game
		   || WorldType == EWorldType::PIE
		   || WorldType == EWorldType::Editor;
}

void USDFResourceSubsystem::ResetSDF() const
{
	if (SceneViewExtension)
	{
		SceneViewExtension->Reset();
	}
}

void USDFResourceSubsystem::Initialize( FSubsystemCollectionBase& Collection )
{
	Super::Initialize( Collection );

	SceneViewExtension =
			FSceneViewExtensions::NewExtension<FSDFExtension>( GetWorld() );
}

void USDFResourceSubsystem::Deinitialize()
{
	if (SceneViewExtension)
	{
		FSDFExtension* SVE = SceneViewExtension.Get();
		ENQUEUE_RENDER_COMMAND( ReleaseSDF )( [SVE]( FRHICommandListImmediate& )
		{
			SVE->ReleaseResources();
		} );
		FlushRenderingCommands();
		SceneViewExtension.Reset();
	}
	Super::Deinitialize();
}