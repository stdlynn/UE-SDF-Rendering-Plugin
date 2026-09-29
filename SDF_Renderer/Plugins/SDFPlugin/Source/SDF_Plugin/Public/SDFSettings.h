// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SDFSettings.generated.h"

UCLASS( Config = Game, DefaultConfig, meta = (DisplayName = "SDF Settings") )
class SDFPLUGIN_API USDFSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	// TODO: default
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|FBM",
			   meta = (DisplayName = "Num Octaves") )
	uint32 NumOctaves{ 3 };

	UPROPERTY( EditAnywhere, Config, Category = "Terrain|fBM",
			   meta = (DisplayName = "Frequency") )
	float Frequency{ 1.0 };

	UPROPERTY( EditAnywhere, Config, Category = "Terrain|fBM",
			   meta = (DisplayName = "Lacunarity") )
	float Lacunarity{ 2.0 };

	UPROPERTY( EditAnywhere, Config, Category = "Terrain|fBM",
			   meta = (DisplayName = "Amplitude (sphere radius)") )
	float Amplitude{ 0.5 };

	UPROPERTY( EditAnywhere, Config, Category = "Terrain|fBM",
			   meta = (DisplayName = "Persistence") )
	float Persistence{ 0.5 };

	UPROPERTY( EditAnywhere, Config, Category = "Terrain",
			   meta = (DisplayName = "Base Radius") )
	float BaseRadius{ 0.0 };

	UPROPERTY( EditAnywhere, Config, Category = "Terrain",
			   meta = (DisplayName = "Plane Z Coord") )
	float PlaneZ{ 0.0 };

	UPROPERTY( EditAnywhere, Config, Category = "Terrain",
			   meta = (DisplayName = "Plane Z Coord") )
	float BaseInflation{ 0.0 };

	// TODO: inflation multiplier
	// TODO: radius randomness thing (e.g. if 1.2, use '1-(1.2-1)' to find lower range, clamp [1,2])
	// actually clamp it [0,1] and do +1 in shader)
	// TODO: K1 and K2

	// ========================
	// fBM above, other beneath
	// ========================

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

	virtual FName GetCategoryName() const override { return TEXT( "Plugins" ); }

	// TODO: add PtrGridSize (uint3) and AtlasCapacity (measured in bricks)

}; // USDFSettings