#include "SDFEdits.h"

// Sets default values
ASDFActor::ASDFActor()
	: SDFComp{ CreateDefaultSubobject<USDFComponent>( TEXT( "SDF Edit" ) ) }
{
	// Use the SDF object as the root component so it dictates the actor's transform
	RootComponent = SDFComp;

	// Do we need this? Sure don't hope so
	PrimaryActorTick.bCanEverTick = false;
}

USDFComponent::USDFComponent()
	: ID{ NextID++ }
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USDFComponent::OnRegister()
{
	Super::OnRegister();
	if (USDFSubsystem* SDFSubsystem{ GetWorld()->GetSubsystem<USDFSubsystem>() })
	{
		SDFSubsystem->RegisterSDF( *this );
	}
}

void USDFComponent::OnUnregister()
{
	if (USDFSubsystem* SDFSubsystem{ GetWorld()->GetSubsystem<USDFSubsystem>() })
	{
		SDFSubsystem->UnregisterSDF( *this );
	}
	Super::OnUnregister();
}

#if WITH_EDITOR
void USDFComponent::PostEditComponentMove( bool bFinished )
{
	Super::PostEditComponentMove( bFinished );
	SetDirty();
}
#endif

void USDFComponent::OnUpdateTransform( EUpdateTransformFlags TransformFlags,
									   ETeleportType TeleportType )
{
	Super::OnUpdateTransform( TransformFlags, TeleportType );
	SetDirty();
}

void USDFComponent::SetDirty() const
{
	if (USDFSubsystem* SDFSubsystem{ GetWorld()->GetSubsystem<USDFSubsystem>() })
	{
		SDFSubsystem->SetSDFDirty( *this );
	}
}

bool USDFSubsystem::DoesSupportWorldType( const EWorldType::Type WorldType ) const
{
	return WorldType == EWorldType::Type::Game
		   || WorldType == EWorldType::Type::PIE // Play in editor
		   || WorldType == EWorldType::Type::Editor;
}

void USDFSubsystem::RegisterSDF( const USDFComponent& Comp )
{
	const FVector3f Pos{ Comp.GetComponentTransform().GetLocation() };
	RenderSDFs.Add( Comp.GetID(), { Comp.GetSDF(), Pos } );
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
	const FVector3f Pos{ Comp.GetComponentTransform().GetLocation() };
	RenderSDFs[Comp.GetID()] = FRenderSDF{ Comp.GetSDF(), Pos };
	bDirty = true;
}