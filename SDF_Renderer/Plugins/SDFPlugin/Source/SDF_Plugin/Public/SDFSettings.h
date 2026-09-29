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

	UPROPERTY( EditAnywhere, Config, Category = "Terrain",
			   meta = (DisplayName = "Plane Z Coord") )
	float PlaneZ{ 0.0f };

	// TODO: default
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|FBM",
			   meta = (DisplayName = "Num Octaves",
						ClampMin = "1") )
	uint32 NumOctaves{ 3 };

	UPROPERTY( EditAnywhere, Config, Category = "Terrain|FBM",
			   meta = (DisplayName = "Frequency",
						ClampMin = "0.1") )
	float Frequency{ 1.0f };

	UPROPERTY( EditAnywhere, Config, Category = "Terrain|FBM",
			   meta = (DisplayName = "Lacunarity",
						ClampMin = "1.0") )
	float Lacunarity{ 2.0f };

	// TODO: default
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|FBM",
			   meta = (DisplayName = "Amplitude (sphere radius)",
						ClampMin = "0.1") )
	float Amplitude{ 1.0f };

	UPROPERTY( EditAnywhere, Config, Category = "Terrain|FBM",
			   meta = (DisplayName = "Persistence",
						ClampMin = "0.1",
						ClampMax = "1.0") )
	float Persistence{ 0.5f };

	// TODO: default
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|Blending",
			   meta = (DisplayName = "Inflation Percentage",
						ClampMin = "1.0") )
	float Inflation{ 1.0f };

	// TODO: default
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|Blending",
			   meta = (DisplayName = "Inflation Gain",
						ClampMin = "0.1",
						ClampMax = "1.0") )
	float InflationGain{ 1.0f };

	// TODO: default
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|Blending",
			   meta = (DisplayName = "Union Smoothing",
						ClampMin = "1.0") )
	float UnionSmoothing{ 0.0f };

	// TODO: default
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|Blending",
			   meta = (DisplayName = "Clipping Smoothing",
						ClampMin = "1.0") )
	float ClippingSmoothing{ 1.0f };

	// TODO: default
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|Blending",
			   meta = (DisplayName = "Max. Radius Variance (%)",
						ClampMin = "0.0",
						ClampMax = "1.0") )
	float MaxRadiusVariance{ 0.15f };

	// TODO: radius randomness thing (e.g. if 1.2, use '1-(1.2-1)' to find lower range, clamp [1,2])
	// actually clamp it [0,1] and do +1 in shader)

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

	// True if any setting is dirty (NOT materials)
	bool bSettingDirty = true;

	virtual FName GetCategoryName() const override { return TEXT( "Plugins" ); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// TODO: add PtrGridSize (uint3) and AtlasCapacity (measured in bricks)

}; // USDFSettings