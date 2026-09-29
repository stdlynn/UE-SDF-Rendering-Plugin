// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// Plugin
#include "SDFMaterial.h"

// UE
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SDFSettings.generated.h"

UCLASS( Config = Game, DefaultConfig, meta = (DisplayName = "SDF Settings") )
class SDFPLUGIN_API USDFSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UPROPERTY( EditAnywhere, Config, Category = "Grid",
			   meta = (DisplayName = "Grid Pos") )
	FVector3f PtrGridPos{ FVector3f::ZeroVector };

	UPROPERTY( EditAnywhere, Config, Category = "Grid",
		meta = (DisplayName = "Voxel Size",
				 ClampMin = "0.01") )
	float VoxSize{ 100.0f / 7.0f };

	UPROPERTY( EditAnywhere, Config, Category = "Debug Visualizations",
		meta = (DisplayName = "Max. Bricks in Trace",
			ClampMin = "1") )
	uint32 DbgVisMaxBricks{ 50 };

	UPROPERTY( EditAnywhere, Config, Category = "Debug Visualizations",
			   meta = (DisplayName = "Blend Factor",
						ClampMin = "0",
						ClampMax = "1" ) )
	float DbgVisBlendFactor{ 0.6f };

	UPROPERTY( EditAnywhere, Config, Category = "Rendering",
			   meta = (DisplayName = "Surface Epsilon",
						UIMin = "0.01",
						UIMax = "1.0") )
	float SurfaceEpsilon{ 5e-1f };

	UPROPERTY( EditAnywhere, Config, Category = "Rendering",
			   meta = (DisplayName = "Derivative H",
						UIMin = "0.1",
						UIMax = "1.0") )
	float DerivativeH{ 4e-2f };
	
	UPROPERTY( EditAnywhere, Config, Category = "Rendering",
		meta = (DisplayName = "GPU Unit Scale",
			UIMin = "0.001",
			UIMax = "0.1") )
	float GPUUnitScale{ 1e-2f };

	UPROPERTY(EditAnywhere, Config, Category = "Rendering",
		meta = (DisplayName = "Materials"))
	TArray<FSDFMaterial> Materials;

	bool bMaterialsDirty = true;

	virtual FName GetCategoryName() const override { return TEXT( "Plugins" ); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// TODO: add PtrGridSize (uint3) and AtlasCapacity (measured in bricks)

}; // USDFSettings