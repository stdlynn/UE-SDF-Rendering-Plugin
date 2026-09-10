// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SDFEdits.generated.h"

struct FSDFGameParams
{
	float Radius{ 100.0f };
};

struct FSDFRenderParams
{
	FVector3f Pos;
	float Radius;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class USDFComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	USDFComponent();

	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	const FSDFGameParams& GetParams() const { return Params; }
	uint64 GetID() const { return ID; }

protected:

	virtual void PostEditComponentMove( bool bFinished ) override;
	virtual void OnUpdateTransform( EUpdateTransformFlags TransformFlags,
									ETeleportType TeleportType = ETeleportType::None ) override;

private:

	inline static uint64 NextID{ 0 };

	FSDFGameParams Params{};
	uint64 ID;

}; // USDFComponent

UCLASS()
class ASDFActor : public AActor
{
	GENERATED_BODY()

public:

	// Sets default values for this actor's properties
	ASDFActor();

private:

	USDFComponent* sdf_object;

}; // ASDFActor

UCLASS()
class USDFSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	void RegisterSDF( const USDFComponent& Comp );
	void UnregisterSDF( const USDFComponent& Comp );
	void SetSDFDirty( const USDFComponent& Comp );

	const TMap<uint64, FSDFRenderParams>& GetRenderParams() const { return SDFRenderParams; }
	bool GetAndResetDirty()
	{
		const bool bWasDirty{ bDirty };
		bDirty = false;
		return bWasDirty;
	}

private:

	// Game thread
	TMap<uint64, FSDFRenderParams> SDFRenderParams;
	bool bDirty{ true };

}; // USDFSubsystem