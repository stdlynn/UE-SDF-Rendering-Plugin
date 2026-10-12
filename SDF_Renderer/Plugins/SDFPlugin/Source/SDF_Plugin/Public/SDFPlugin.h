// Custom SceneViewExtension Template for Unreal Engine
// Copyright 2023 - 2025 Ossi Luoto
// 
// Main Module to set Shader Directories

#pragma once

// UE
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "SDFPlugin.generated.h"

class FSDFPlugin : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

UCLASS()
class SDFPLUGIN_API USDFPluginLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "SDFPlugin", meta = (WorldContext = "WorldContextObject"))
	static void SpawnAdditive(UObject* WorldContextObject, FVector Location);
	UFUNCTION(BlueprintCallable, Category = "SDFPlugin", meta = (WorldContext = "WorldContextObject"))
	static void SpawnSubtractive(UObject* WorldContextObject, FVector Location);
};