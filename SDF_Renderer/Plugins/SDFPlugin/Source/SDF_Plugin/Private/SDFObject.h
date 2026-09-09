// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SDFObject.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class USDFObject : public USceneComponent
{
	GENERATED_BODY()

public:

	// Sets default values for this component's properties
	USDFObject();

	// Called every frame
	virtual void TickComponent( float DeltaTime, 
								ELevelTick TickType, 
								FActorComponentTickFunction* ThisTickFunction ) override;

	float GetRadius() const { return Radius; }

protected:

	// Called when the game starts
	virtual void BeginPlay() override;

private:

	// Only circle for now...
	float Radius{ 100.0f };

};