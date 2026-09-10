// Fill out your copyright notice in the Description page of Project Settings.

#include "SDFComponent.h"

USDFComponent::USDFComponent()
{
	// TODO: do we need this?
	// TODO: Does this affect transforms?
	PrimaryComponentTick.bCanEverTick = true;
}

void USDFComponent::OnRegister()
{
	Super::OnRegister();
	GetWorld()->GetSubsystem<USDFSubsystem>()->RegisterSDF( *this );
}

void USDFComponent::OnUnregister()
{
	GetWorld()->GetSubsystem<USDFSubsystem>()->UnregisterSDF( *this );
	Super::OnUnregister();
}

void USDFComponent::OnUpdateTransform( EUpdateTransformFlags TransformFlags,
									   ETeleportType TeleportType )
{
	Super::OnUpdateTransform( TransformFlags, TeleportType );
	GetWorld()->GetSubsystem<USDFSubsystem>()->SetSDFDirty( *this );
}

void USDFSubsystem::RegisterSDF( const USDFComponent& Comp )
{
	const auto [Radius]{ Comp.GetParams() };
	const FVector3f Pos{ Comp.GetComponentTransform().GetLocation() };
	SDFRenderParams.Emplace( Pos, Radius );
	// TODO: keep track of components (I think we just assign IDs upon construction?)
	bDirty = true;

	// Debug
	GEngine->AddOnScreenDebugMessage( -1, // Force newline
									  5.0f, // Time
									  FColor::Yellow, 
									  FString::Printf( TEXT( "Added new SDF (%d total)" ), SDFRenderParams.Num() ) );
}

void USDFSubsystem::UnregisterSDF( const USDFComponent& Comp )
{
	//
	bDirty = true;

	// Debug
	GEngine->AddOnScreenDebugMessage( -1, // Force newline
									  5.0f, // Time
									  FColor::Yellow, 
									  FString::Printf( TEXT( "Removed SDF (%d total)" ), SDFRenderParams.Num() ) );
}

void USDFSubsystem::SetSDFDirty( const USDFComponent& Comp )
{
	//
	bDirty = true;
}