#include "SDFEdits.h"

namespace
{
	TAutoConsoleVariable CVarDebugWireframe(TEXT("r.SDF.Debug.Wireframe"),
		0,
		TEXT("Enable debug view of subtractive edits \n")
		TEXT(" 0: OFF;")
		TEXT(" 1: Subtractive only;")
		TEXT(" 2: ALL"));
}

// Sets default values
ASDFActor::ASDFActor()
	: SDFComp{ CreateDefaultSubobject<USDFComponent>( TEXT( "SDF Edit" ) ) }
{
	// Use the SDF object as the root component so it dictates the actor's transform
	RootComponent = SDFComp;

	// Do we need this? Sure don't hope so
	PrimaryActorTick.bCanEverTick = false;
}

void USDFSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DrawDebug();
}

FGPUSDF::FGPUSDF( const FRenderSDF& RenderSDF, uint32 ID, float GPUUnitScale )
	: Pos{ RenderSDF.Pos * GPUUnitScale }
	, Flags { 0 }
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
			SetBoxData( FVector3f(RenderSDF.Data) * GPUUnitScale, RenderSDF.Quat, RenderSDF.Data.W * GPUUnitScale );
			break;
		}
	}
}

void FGPUSDF::SetPrimitive( const EPrimitive NewPrimitive )
{
	static constexpr uint32 Mask = 0b11;
	Flags = (Flags & ~Mask) | (static_cast<uint32>( NewPrimitive ) & Mask);
}

void FGPUSDF::SetSphereData( const float Radius )
{
	Data0.X = Radius;
}

void FGPUSDF::SetLineData( const FVector3f& B, const float Radius )
{
	Data0 = FVector4f(B, Radius);
}

void FGPUSDF::SetBoxData( const FVector3f& HalfExtent, const FQuat& Quat, const float Rounding )
{
	Data0 = FVector4f(HalfExtent, Rounding);
	Data1 = FVector4f(Quat.X, Quat.Y, Quat.Z, Quat.W);
}

void FGPUSDF::SetOperation( const EOperation NewOperation )
{
	static constexpr uint32 Mask = 0b100;
	Flags = (Flags & ~Mask) | ((static_cast<uint32>(NewOperation) << 2) & Mask);
}

void FGPUSDF::EnableSmoothing( const bool Enabled )
{
	static constexpr uint32 Mask = 0b1000;
	Flags = (Flags & ~Mask) | (Enabled ? Mask : 0u);
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
	const FQuat Quat{ Comp.GetComponentTransform().GetRotation() };
	RenderSDFs.Add( Comp.GetID(), { Comp.GetSDF(), Pos, Quat } );
	bDirty = true;
}

void USDFSubsystem::UnregisterSDF( const USDFComponent& Comp )
{
	RenderSDFs.Remove( Comp.GetID() );
	bDirty = true;
}

void USDFSubsystem::SetSDFDirty( const USDFComponent& Comp )
{
	const FVector3f Pos{ Comp.GetComponentTransform().GetLocation() };
	const FQuat Quat{ Comp.GetComponentTransform().GetRotation() };
	RenderSDFs[Comp.GetID()] = FRenderSDF{ Comp.GetSDF(), Pos, Quat };
	bDirty = true;
}

// Render wireframe of subtractive edits so we can see them
void USDFSubsystem::DrawDebug()
{
	int CVar = CVarDebugWireframe.GetValueOnGameThread();
	if (!CVar) return;
	UWorld* World = GetWorld();
	if (!World) return;

	for (const auto& [ID, Edit] : RenderSDFs)
	{
		float SizeEpsilon = 1e-2;
		float SizeMult = 3.5e-2;
		FColor Color = FColor::Green;
		EOperation Op = Edit.Operation;
		
		if (Op == EOperation::Add)
		{
			SizeMult += 1.0f;
			if (CVar < 2) continue;
		}
		else
		{
			// Invert epsilon for subtractive edits because they're on the inside
			SizeEpsilon *= -1.0f;
			SizeMult = 1.0f - SizeMult;

			Color = FColor::Red;
		}

		switch (Edit.Primitive)
		{
			default:
			case EPrimitive::Sphere:
			{
				// Account for sphere's limited vertex resolution making it clip inside
				const float Radius = Edit.GetSphereRadius() * SizeMult;
				DrawDebugSphere( World, static_cast<FVector>(Edit.Pos),
								 Radius, 16, Color );
				break;
			}
			case EPrimitive::Line:
			{
				const FVector PosA = static_cast<FVector>(Edit.Pos);
				const FVector PosB = PosA + static_cast<FVector>(Edit.GetLinePosB());
				const FVector Center = (PosA + PosB) / 2.0f;
				const float Radius = Edit.GetLineRadius() * SizeMult;
				const float HalfHeight = FVector::Dist( PosA, PosB ) * 0.5f + Radius;

				const FVector Dir = (PosB - PosA).GetSafeNormal();
				const FQuat Rotation = FQuat::FindBetweenNormals( FVector::UpVector, Dir );

				DrawDebugCapsule( World, Center, HalfHeight, Radius,
								  Rotation, Color );
				break;
			}
			case EPrimitive::Box:
			{
				DrawDebugBox( World, static_cast<FVector>(Edit.Pos),
							  static_cast<FVector>( Edit.GetBoxHalfExtent() ) + SizeEpsilon,
							  Edit.Quat, Color );
				break;
			}
		}
	}
}