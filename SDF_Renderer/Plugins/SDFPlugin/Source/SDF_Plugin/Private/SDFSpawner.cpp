#include "SDFSpawner.h"

static TAutoConsoleVariable<int32> CVarSDFCount(
    TEXT("r.SDF.ProfileCount"), -1,
    TEXT("Number of spheres to spawn for performance testing"));

void ASDFSpawner::BeginPlay()
{
    Super::BeginPlay();

    const int32 Override = CVarSDFCount.GetValueOnGameThread();
    const int32 N = Override >= 0 ? Override : Count;

    FRandomStream Rand{ Seed };
    const FVector Origin = GetActorLocation();

    for (int32 i = 0; i < N; ++i)
    {
        const FVector Offset{
            Rand.FRandRange(-Extent.X, Extent.X),
            Rand.FRandRange(-Extent.Y, Extent.Y),
            Rand.FRandRange(-Extent.Z, Extent.Z) };

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        ASDFActor* Actor = GetWorld()->SpawnActor<ASDFActor>(
            ASDFActor::StaticClass(), Origin + Offset, FRotator::ZeroRotator, Params);
    }
}