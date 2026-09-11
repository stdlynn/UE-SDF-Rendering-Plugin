// Fill out your copyright notice in the Description page of Project Settings.

#include "SDFEdits.h"

// Sets default values
ASDFActor::ASDFActor()
	: SDFComp{ CreateDefaultSubobject<USDFComponent>( TEXT( "SDF Edit" ) ) }
{
	// Use the SDF object as the root component so it dictates the actor's transform
	RootComponent = SDFComp;

	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

USDFComponent::USDFComponent()
	: ID{ NextID++ }
{
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

void USDFComponent::PostEditComponentMove( bool bFinished )
{
	Super::PostEditComponentMove( bFinished );
	GetWorld()->GetSubsystem<USDFSubsystem>()->SetSDFDirty( *this );
}

void USDFComponent::OnUpdateTransform( EUpdateTransformFlags TransformFlags,
									   ETeleportType TeleportType )
{
	Super::OnUpdateTransform( TransformFlags, TeleportType );
	GetWorld()->GetSubsystem<USDFSubsystem>()->SetSDFDirty( *this );
}

void USDFSubsystem::RegisterSDF( const USDFComponent& Comp )
{
	const auto [Radius]{ Comp.GetSDF() };
	const FVector3f Pos{ Comp.GetComponentTransform().GetLocation() };
	RenderSDFs.Add( Comp.GetID(), { Pos, Radius } );
	bDirty = true;

	// Debug
	GEngine->AddOnScreenDebugMessage( -1, // Force newline
									  5.0f, // Time
									  FColor::Green, 
									  FString::Printf( TEXT( "[+] SDF Edit (%d total)" ), RenderSDFs.Num() ) );
}

void USDFSubsystem::UnregisterSDF( const USDFComponent& Comp )
{
	RenderSDFs.Remove( Comp.GetID() );
	bDirty = true;

	// Debug
	GEngine->AddOnScreenDebugMessage( -1, // Force newline
									  5.0f, // Time
									  FColor::Red, 
									  FString::Printf( TEXT( "[-] SDF Edit (%d total)" ), RenderSDFs.Num() ) );
}

void USDFSubsystem::SetSDFDirty( const USDFComponent& Comp )
{
	const auto [Radius]{ Comp.GetSDF() };
	const FVector3f Pos{ Comp.GetComponentTransform().GetLocation() };
	RenderSDFs[Comp.GetID()] = FRenderSDF{ Pos, Radius };
	bDirty = true;
}