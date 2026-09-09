// Custom SceneViewExtension Template for Unreal Engine
// Copyright 2023 - 2025 Ossi Luoto
// 
// Subsystem to keep custom SceneViewExtension alive

#include "TemplateSubsystem.h"
#include "SDFSceneViewExtension.h"
#include "SceneViewExtension.h"

void UTemplateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	SDFSceneViewExtension = FSceneViewExtensions::NewExtension<FSDFSceneViewExtension>();
	UE_LOG(LogTemp, Log, TEXT("SDFPlugin: Subsystem initialized & SceneViewExtension created"));
}

void UTemplateSubsystem::Deinitialize()
{
	{
		SDFSceneViewExtension->IsActiveThisFrameFunctions.Empty();

		FSceneViewExtensionIsActiveFunctor IsActiveFunctor;

		IsActiveFunctor.IsActiveFunction = [](const ISceneViewExtension* SceneViewExtension, const FSceneViewExtensionContext& Context)
		{
			return TOptional<bool>(false);
		};

		SDFSceneViewExtension->IsActiveThisFrameFunctions.Add(IsActiveFunctor);
	}

	SDFSceneViewExtension.Reset();
	SDFSceneViewExtension = nullptr;
}
