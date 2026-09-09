// Fill out your copyright notice in the Description page of Project Settings.

#include "SDFComponent.h"

USDFComponent::USDFComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

void USDFComponent::OnRegister()
{
	Super::OnRegister();
	GetWorld()->GetSubsystem<USDFSubsystem>()->RegisterSDF( this );
}

void USDFComponent::OnUnregister()
{
	GetWorld()->GetSubsystem<USDFSubsystem>()->UnregisterSDF( this );
	Super::OnUnregister();
}

void USDFComponent::OnUpdateTransform( EUpdateTransformFlags,
									   ETeleportType )
{
	GetWorld()->GetSubsystem<USDFSubsystem>()->SetSDFDirty( this );
	// TODO:
}

void USDFSubsystem::RegisterSDF( USDFComponent* Comp )
{
	const FSDFGameParams Params{ Comp->GetParams() };
	const FVector3f Pos{ Comp->GetComponentTransform().GetLocation() };
	SDFRenderParams.Emplace( Pos, Params.Radius );
	// TODO: keep track of components
	bDirty = true;
}

void USDFSubsystem::UnregisterSDF( USDFComponent* Comp )
{
	//
	bDirty = true;
}

void USDFSubsystem::SetSDFDirty( USDFComponent* Comp )
{
	//
	bDirty = true;
}

void USDFSubsystem::Tick( float DeltaTime )
{
	if (bDirty)
	{
		TransferToRenderThread();
		bDirty = false;
	}
}

void USDFSubsystem::TransferToRenderThread()
{
	// Transfer lambda
	// Move the render data, since it will go out of scope before this is executed
	auto TransferParams{ [this, Params{ MoveTemp( SDFRenderParams ) }]( FRHICommandList& ) mutable
	{
		// On the render thread, set the SDF render data
		SDFRenderParams_RenderThread = MoveTemp( Params );
	} };

	// Perform the transfer safely on the render thread
	ENQUEUE_RENDER_COMMAND( SDFParamsThreadTransfer )( TransferParams );
}