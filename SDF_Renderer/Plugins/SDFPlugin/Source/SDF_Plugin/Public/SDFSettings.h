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

	// Sample 4 tetrahedron corners instead of 8 grid corners
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|Noise",
			   meta = (DisplayName = "Use Simplex Noise") )
	bool bSimplex{ false };

	UPROPERTY( EditAnywhere, Config, Category = "Terrain|Noise",
			   meta = (DisplayName = "Plane Position") )
	FVector3f PlanePos{ 0.0f, 0.0f, 0.0f };

	// Relative to the diameter (amplitude)
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|Noise",
			   meta = (DisplayName = "Vertical Offset",
						ClampMin = "-0.5",
						ClampMax = "0.5") )
	float VerticalNoiseOffset{ 0.0f };

	// TODO: implement & default
	// Relative to the diameter of a sample sphere
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|Noise",
			   meta = (DisplayName = "Min. Noise Scale (%)",
						ClampMin = "0.0",
						UIMax = "1.0") )
	float MinNoiseScale{ 0.5f };

	// Normalized before use
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|Noise",
			   meta = (DisplayName = "Rotation Axis",
						ClampMin = "0.0",
						ClampMax = "1.0") )
	FVector3f RotAxis{ 1.0f, 1.0f, 1.0f };

	UPROPERTY( EditAnywhere, Config, Category = "Terrain|Noise",
			   meta = (DisplayName = "Rotation Angle",
						ClampMin = "0.0",
						ClampMax = "360.0") )
	float RotAngle{ 54.0f };

	UPROPERTY( EditAnywhere, Config, Category = "Terrain|FBM",
			   meta = (DisplayName = "Num Octaves",
						ClampMin = "0") )
	uint32 NumOctaves{ 11 };

	UPROPERTY( EditAnywhere, Config, Category = "Terrain|FBM",
			   meta = (DisplayName = "Frequency (m)",
						ClampMin = "0.0001",
						UIMin = "0.1") )
	float Frequency{ 1.0f };

	// Frequency gain (per octave)
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|FBM",
			   meta = (DisplayName = "Lacunarity",
						ClampMin = "0.0",
						UIMin = "1.0") )
	float Lacunarity{ 2.0f };

	// The maximum diameter of the first octave of noise, relative to the frequency
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|FBM",
			   meta = (DisplayName = "Amplitude (m)",
						ClampMin = "0.0001",
						UIMin = "0.1",
						UIMax = "1.0") )
	float Amplitude{ 1.0f };

	// Amplitude gain (per octave)
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|FBM",
			   meta = (DisplayName = "Persistence",
						ClampMin = "0.0001",
						UIMax = "1.0") )
	float Persistence{ 0.5f };

	// TODO: default
	// Inflation distance relative to the maximum height of the first octave
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|Blending",
			   meta = (DisplayName = "Inflation (%)",
						ClampMin = "0.0",
						UIMax = "1.0") )
	float Inflation{ 0.06f };

	// TODO: default
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|Blending",
			   meta = (DisplayName = "Inflation Gain",
						ClampMin = "0.0",
						UIMax = "1.0") )
	float InflationGain{ 1.0f };

	// TODO: default
	// Relative to the diameter of a sample sphere
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|Blending",
			   meta = (DisplayName = "Sample Smoothing (%)",
						ClampMin = "0.0",
						UIMin = "0.0001",
						UIMax = "0.1") )
	float SampleSmoothing{ 0.01f };

	// TODO: default
	// Relative to the diameter of a sample sphere
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|Blending",
			   meta = (DisplayName = "Clipped Sphere Smoothing (%)",
						ClampMin = "0.0",
						UIMin = "0.0001",
						UIMax = "1.0") )
	float ClippingSmoothing{ 0.1f };

	// TODO: default
	// Relative to the diameter of a sample sphere
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|Blending",
			   meta = (DisplayName = "Terrain Smoothing (%)",
						ClampMin = "0.0",
						UIMin = "0.0001",
						UIMax = "1.0") )
	float UnionSmoothing{ 0.1f };

	// Eliminate aliasing artefacts by not rendering sub-pixel details
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|BruteForceRendering",
			   meta = (DisplayName = "Band-limiting") )
	bool bBandLimiting{ true };

	// The maximum distance from a surface to still count as a hit
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|BruteForceRendering",
			   meta = (DisplayName = "Surface Epsilon",
						UIMin = "0.0001",
						UIMax = "0.01") )
	float TerrainEpsilon{ 1e-4 };

	// The 'h' in L'Hôpital's rule
	UPROPERTY( EditAnywhere, Config, Category = "Terrain|BruteForceRendering",
			   meta = (DisplayName = "Normals' Smoothing",
						ClampMin = "0.0",
						UIMin = "0.01",
						UIMax = "1.0") )
	float TerrainDerivativeH{ 4e-2f };

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

	// The amount of brick hits necessary for a pixel to render white
	UPROPERTY( EditAnywhere, Config, Category = "Debug Visualizations",
		meta = (DisplayName = "Max. Bricks in Trace",
			ClampMin = "1") )
	uint32 DbgVisMaxBricks{ 50 };

	// Blending between the rendered scene and the debug visualization
	UPROPERTY( EditAnywhere, Config, Category = "Debug Visualizations",
			   meta = (DisplayName = "Blend Factor",
						ClampMin = "0",
						ClampMax = "1" ) )
	float DbgVisBlendFactor{ 0.6f };

	// The maximum distance from a surface to still count as a hit
	UPROPERTY( EditAnywhere, Config, Category = "Rendering",
			   meta = (DisplayName = "Surface Epsilon",
						UIMin = "0.01",
						UIMax = "1.0") )
	float SurfaceEpsilon{ 5e-1f };

	// The 'h' in L'Hôpital's rule
	UPROPERTY( EditAnywhere, Config, Category = "Rendering",
			   meta = (DisplayName = "Normals' Smoothing",
						ClampMin = "0.0",
						UIMin = "0.1",
						UIMax = "1.0") )
	float DerivativeH{ 4e-2f };
	
	// Positions and scales will be multiplied by this when doing calculations on the GPU
	UPROPERTY( EditAnywhere, Config, Category = "Rendering",
		meta = (DisplayName = "GPU Unit Scale",
			ClampMin = "0.0",
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