#include "SDFEdits.h"
#include "BrickmapCommon.h"
#include "SDFSettings.h"

namespace
{
	TAutoConsoleVariable CVarDebugWireframe(TEXT("r.SDF.Debug.Wireframe"),
		0,
		TEXT("Enable debug view of subtractive edits \n")
		TEXT(" 0: OFF;")
		TEXT(" 1: Subtractive only;")
		TEXT(" 2: ALL"));

	TAutoConsoleVariable CVarDebugBounds(TEXT("r.SDF.Debug.Bounds"),
		0,
		TEXT("Enable debug view of the brickmap bounds \n")
		TEXT(" 0: OFF;")
		TEXT(" 1: ON"));
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

	BVH.Build( Edits.Array() );

	DrawDebug();
}

void FSDF::CalcBounds()
{
	switch (Primitive)
	{
		case EPrimitive::Sphere: 
		{
			Bounds.Min = Pos - GetLineRadius();
			Bounds.Max = Pos + GetLineRadius();
			break;
		}
		case EPrimitive::Line: 
		{
			Bounds.Min = Pos.ComponentMin( GetLinePosB() ) - GetLineRadius();
			Bounds.Max = Pos.ComponentMin( GetLinePosB() ) + GetLineRadius();
			break;
		}
		case EPrimitive::Box:
		{
			// Scale the box down by the smoothing radius
			const float SmoothDist = bSmooths ? SmoothingStrength : 0.0f;
			const FVector3f HalfExtent{ GetBoxHalfExtent() - SmoothDist };

			// [CREDITS] Graphics Gems by Jim Arvo
			// Rotate locally (note that Unreal Engine uses row-major)
			const FMatrix Mat{ Quat.ToMatrix() };
			const FVector3f Col0{ FVector3f{ Mat.GetColumn( 0 ) }.GetAbs() * HalfExtent };
			const FVector3f Col1{ FVector3f{ Mat.GetColumn( 1 ) }.GetAbs() * HalfExtent };
			const FVector3f Col2{ FVector3f{ Mat.GetColumn( 2 ) }.GetAbs() * HalfExtent };
			const FVector3f WorldSpaceHalfExtent{ (Col0.X + Col0.Y + Col0.Z), (Col1.X + Col1.Y + Col1.Z), (Col2.X + Col2.Y + Col2.Z) };
			// Set world-space bounds
			Bounds.Min = Pos - WorldSpaceHalfExtent;
			Bounds.Max = Pos + WorldSpaceHalfExtent;

			// Expand the AABB in all directions by the smoothing distance
			Bounds.Min -= FVector3f{ SmoothDist };
			Bounds.Max += FVector3f{ SmoothDist };
			break;
		}
	}
}

FGPUSDF::FGPUSDF( const FSDF& RenderSDF, uint32 ID, float GPUUnitScale )
	: Pos{ RenderSDF.Pos * GPUUnitScale }
	, Flags { 0 }
	, SmoothingStrength{ RenderSDF.SmoothingStrength * GPUUnitScale }
	, ID{ ID }
	, MatIdx{ RenderSDF.MatIdx }
{
	SetPrimitive( RenderSDF.Primitive );
	SetOperation( RenderSDF.Operation );
	SetColorsSubtraction( RenderSDF.bColorsSubtraction );
	EnableSmoothing( RenderSDF.bSmooths );

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

void FGPUSDF::SetColorsSubtraction( const bool bEnabled)
{
	static constexpr uint32 Mask = 0b10000;
	Flags = (Flags & ~Mask) | (bEnabled ? Mask : 0u);
}

void FGPUSDF::EnableSmoothing( const bool bEnabled )
{
	static constexpr uint32 Mask = 0b1000;
	Flags = (Flags & ~Mask) | (bEnabled ? Mask : 0u);
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
	Edits.Add( Comp.GetID(), { Comp, Pos, Quat } );
	bDirty = true;
}

void USDFSubsystem::UnregisterSDF( const USDFComponent& Comp )
{
	Edits.Remove( Comp.GetID() );
	bDirty = true;
}

void USDFSubsystem::SetSDFDirty( const USDFComponent& Comp )
{
	const FVector3f Pos{ Comp.GetComponentTransform().GetLocation() };
	const FQuat Quat{ Comp.GetComponentTransform().GetRotation() };
	Edits[Comp.GetID()] = FSDF{ Comp, Pos, Quat };
	bDirty = true;
}

// Render wireframe of subtractive edits so we can see them
void USDFSubsystem::DrawDebug()
{
	UWorld* World = GetWorld();
	if (!World) return;

	if (CVarDebugBounds.GetValueOnGameThread())
	{
		USDFSettings* SDFSettings{ GetMutableDefault<USDFSettings>() };
		FVector Center = static_cast<FVector>(SDFSettings->PtrGridPos);
		FVector Extent = static_cast<FVector>(PtrGridSize * SDFSettings->VoxSize * VoxelResolution / 2.0);
		DrawDebugBox( World, Center, Extent, FColor::Magenta );
	}

	int CVarWireframe = CVarDebugWireframe.GetValueOnGameThread();
	if (!CVarWireframe) return;

	for (const auto& [ID, Edit] : Edits)
	{
		float SizeEpsilon = 1e-2f;
		float SizeMult = 3.5e-2f;
		FColor Color = FColor::Green;
		EOperation Op = Edit.Operation;
		
		if (Op == EOperation::Add)
		{
			SizeMult += 1.0f;
			if (CVarWireframe < 2) continue;
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