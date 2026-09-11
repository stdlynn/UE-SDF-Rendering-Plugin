// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SDFEdits.generated.h"

struct FGameSDF
{
	float Radius{ 100.0f };
};

struct FRenderSDF
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
	virtual void PostEditComponentMove( bool bFinished ) override;

	const FGameSDF& GetSDF() const { return GameSDF; }
	uint64 GetID() const { return ID; }

protected:

	virtual void OnUpdateTransform( EUpdateTransformFlags TransformFlags,
									ETeleportType TeleportType = ETeleportType::None ) override;

private:

	inline static uint64 NextID{ 0 };

	// Game params, game thread
	FGameSDF GameSDF{};
	uint64 ID;

}; // USDFComponent

UCLASS()
class ASDFActor : public AActor
{
	GENERATED_BODY()

public:

	ASDFActor();

private:

	USDFComponent* SDFComp;

}; // ASDFActor

UCLASS()
class USDFSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	using FRenderSDFs = TMap<uint64, FRenderSDF>;

	void RegisterSDF( const USDFComponent& Comp );
	void UnregisterSDF( const USDFComponent& Comp );
	void SetSDFDirty( const USDFComponent& Comp );

	const FRenderSDFs& GetRenderParams() const { return RenderSDFs; }
	bool GetAndResetDirty()
	{
		const bool bWasDirty{ bDirty };
		bDirty = false;
		return bWasDirty;
	}

private:

	// Render params, game thread
	FRenderSDFs RenderSDFs;
	bool bDirty{ true };

}; // USDFSubsystem