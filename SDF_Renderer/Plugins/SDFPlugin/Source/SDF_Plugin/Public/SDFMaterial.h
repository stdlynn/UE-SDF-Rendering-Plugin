#pragma once

#include "CoreMinimal.h"
#include "SDFMaterial.generated.h"

USTRUCT( BlueprintType )
struct FSDFMaterial
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (
		ClampMin = "0.0", ClampMax = "1.0"))
	FVector3f Albedo{ 0.6f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (
		ClampMin = "0.0", ClampMax = "1.0"))
	float Roughness{ 0.3f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (
		ClampMin = "0.0", ClampMax = "1.0"))
	FVector3f Emission{ 0.0f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (
		ClampMin = "0.0", ClampMax = "1.0"))
	float Metalness{ 0.0f };
};