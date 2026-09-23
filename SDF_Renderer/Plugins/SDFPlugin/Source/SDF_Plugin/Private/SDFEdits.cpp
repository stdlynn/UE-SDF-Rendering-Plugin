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

FGPUSDF::FGPUSDF( const FRenderSDF& RenderSDF, uint32 ID, float GPUUnitScale )
	: Pos{ RenderSDF.Pos * GPUUnitScale }
	, Flags { 0 }
	, Data{ RenderSDF.Data * GPUUnitScale }
	, SmoothingStrength{ RenderSDF.SmoothingStrength * GPUUnitScale }
	, ID{ ID }
{
	SetPrimitive(RenderSDF.Primitive);
	SetOperation(RenderSDF.Operation);
	EnableSmoothing(RenderSDF.bSmooths);

	switch (RenderSDF.Primitive)
	{
	case EPrimitive::Sphere:
	{
		SetSphereData( RenderSDF.Data.X * GPUUnitScale );
		break;
	}
	case EPrimitive::Line:
	{
		const FVector3f B{ FVector3f( RenderSDF.Data ) * GPUUnitScale };
		SetLineData( B, RenderSDF.Data.W * GPUUnitScale );
		break;
	}
	case EPrimitive::Box:
	{
		SetBoxData( FVector3f(RenderSDF.Data) * GPUUnitScale );
		break;
	}
	default:
	{
		break;
	}
	}
}

void FGPUSDF::SetPrimitive( const EPrimitive NewPrimitive )
{
	static constexpr uint32 Mask = 0b11;
	Flags = (Flags & ~Mask) | (static_cast<uint32>( NewPrimitive ) & Mask);
}

void FGPUSDF::SetSphereData(const float Radius)
{
	Data.X = Radius;
}

void FGPUSDF::SetLineData(const FVector3f& PosB, const float Radius)
{
	Data = FVector4f(PosB, Radius);
}

void FGPUSDF::SetBoxData(const FVector3f& HalfExtent)
{
	Data = FVector4f(HalfExtent, 0);
}

void FGPUSDF::SetOperation(const EOperation NewOperation)
{
	static constexpr uint32 Mask = 0b100;
	Flags = (Flags & ~Mask) | ((static_cast<uint32>(NewOperation) << 2) & Mask);
}

void FGPUSDF::EnableSmoothing(const bool Enabled)
{
	static constexpr uint32 Mask = 0b1000;
	Flags = (Flags & ~Mask) | (Enabled ? Mask : 0u);
}

EPrimitive FGPUSDF::GetPrimitive() const
{
	return static_cast<EPrimitive>( Flags & 0b11 );
}

float FGPUSDF::GetSphereRadius() const
{
	return Data.X;
}

FVector3f FGPUSDF::GetLinePosB() const
{
	return FVector3f( Data.X, Data.Y, Data.Z );
}

float FGPUSDF::GetLineRadius() const
{
	return Data.W;
}

FVector3f FGPUSDF::GetBoxHalfExtent() const
{
	return FVector3f( Data.X, Data.Y, Data.Z );
}

EOperation FGPUSDF::GetSDFOperation() const
{
	return static_cast<EOperation>((Flags >> 2) & 0b1);
}

bool FGPUSDF::GetSDFSmooths() const
{
	return Flags & 0b1000;
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