// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SDFSettings.generated.h"

UCLASS( Config = Game, DefaultConfig, meta = (DisplayName = "SDF Settings"))
class SDFPLUGIN_API USDFSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UPROPERTY( EditAnywhere, Config, Category = "Grid" )
	FVector3f PtrGridPos{ FVector3f::ZeroVector };

	UPROPERTY( EditAnywhere, Config, Category = "Grid" )
	float VoxSize{ 100.0f / 7.0f };

	virtual FName GetCategoryName() const override { return TEXT( "Plugins" ); }

	// TODO: add PtrGridSize (uint3) and AtlasCapacity (measured in bricks)
	
}; // USDFSettings