// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SDFComponent.generated.h"

// TODO: rename SDFObjectActor to SDFActor and move it in here

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

	// Sets default values for this component's properties
	USDFComponent();

	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	const FSDFGameParams& GetParams() const { return Params; }

protected:
	
	// TODO: mark dirty, also if parameters change
	virtual void OnUpdateTransform( EUpdateTransformFlags TransformFlags, 
									ETeleportType TeleportType = ETeleportType::None ) override;

private:

	FSDFGameParams Params{};

};

// TODO: create subsystem to help register the SDFs
UCLASS()
class USDFSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	// TODO: how do we keep track of which is which?
	void RegisterSDF( const USDFComponent& Comp );
	void UnregisterSDF( const USDFComponent& Comp );
	void SetSDFDirty( const USDFComponent& Comp );

	const TArray<FSDFRenderParams>& GetRenderParams() const { return SDFRenderParams; }
	bool GetAndResetDirty()
	{
		const bool bWasDirty{ bDirty };
		bDirty = false;
		return bWasDirty;
	}

private:

	// Game thread
	TArray<FSDFRenderParams> SDFRenderParams;
	bool bDirty{ true };

};