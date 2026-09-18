#pragma once

#include "CoreMinimal.h"
#include "SDFEdits.h"
#include "SDFSpawner.generated.h"

UCLASS()
class ASDFSpawner : public AActor
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, Category = "SDF Profiling")
    int32 Count = 100;

    UPROPERTY(EditAnywhere, Category = "SDF Profiling")
    FVector Extent{ 5000.0f, 5000.0f, 2000.0f };

    UPROPERTY(EditAnywhere, Category = "SDF Profiling")
    float Radius = 100.0f;

    UPROPERTY(EditAnywhere, Category = "SDF Profiling")
    int32 Seed = 123;
};