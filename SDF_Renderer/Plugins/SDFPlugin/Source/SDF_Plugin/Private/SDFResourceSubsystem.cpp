#include "SDFResourceSubsystem.h"
#include "SDFExtension.h"

#include "SDFResourceSubsystem.h"
#include "SDFExtension.h"
#include "SceneViewExtension.h"

bool USDFResourceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	return FApp::CanEverRender();
}

bool USDFResourceSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::Editor;
}

void USDFResourceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SceneViewExtension =
		FSceneViewExtensions::NewExtension<FSDFExtension>(GetWorld());
}

void USDFResourceSubsystem::Deinitialize()
{
	if (SceneViewExtension)
	{
		FSDFExtension* SVE = SceneViewExtension.Get();
		ENQUEUE_RENDER_COMMAND(ReleaseSDF)([SVE](FRHICommandListImmediate&)
			{
				SVE->ReleaseResources();
			});
		FlushRenderingCommands();
		SceneViewExtension.Reset();
	}
	Super::Deinitialize();
}
