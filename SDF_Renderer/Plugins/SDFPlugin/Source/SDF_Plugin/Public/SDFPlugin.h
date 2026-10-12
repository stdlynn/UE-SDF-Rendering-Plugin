// Custom SceneViewExtension Template for Unreal Engine
// Copyright 2023 - 2025 Ossi Luoto
// 
// Main Module to set Shader Directories

#pragma once

// Plugin
#include "SDFEdits.h"

// UE
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "SDFPlugin.generated.h"

USTRUCT(BlueprintType)
struct FEditSettings
{
    GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	EOperation Operation{ EOperation::Add };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	EPrimitive Primitive{ EPrimitive::Sphere };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape",
		meta = (ClampMin = "0.0",
			EditCondition = "Primitive == EPrimitive::Sphere || Primitive == EPrimitive::Line",
			EditConditionHides))
	float Radius{ 100.0f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape",
		meta = (ClampMin = "0.0",
			EditCondition = "Primitive == EPrimitive::Box", EditConditionHides))
	float Rounding{ 0.0f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (
		DisplayName = "Line End Position",
		EditCondition = "Primitive == EPrimitive::Line", EditConditionHides))
	FVector3f LinePosB{ 0.0, 0.0, 100.0 };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape",
		meta = (ClampMin = "0.0",
			DisplayName = "Extent (half)",
			EditCondition = "Primitive == EPrimitive::Box", EditConditionHides))
	FVector3f BoxSize{ 100.0, 100.0, 100.0 };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	bool bSmooths{ false };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "1.0"))
	float Smoothing{ 1.0f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material", meta =
		(ClampMin = "0",
			ClampMax = "255"))
	int MaterialIndex{ 0 };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material", meta = (
		EditCondition = "Operation == EOperation::Subtract",
		EditConditionHides))
	bool bColorsSubtraction{ false };

    // ...Rounding, LinePosB, BoxSize, bSmooths, Smoothing,
    // MaterialIndex, bColorsSubtraction — moved over unchanged
};

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
	static void SpawnSDF( const UObject* WorldContextObject, const FVector& Location, const USDFComponent* Settings );
};