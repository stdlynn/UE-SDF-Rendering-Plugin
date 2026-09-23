#pragma once

// UE
#include "CoreMinimal.h"

// Plugin
#include "SDFEdits.h"

// Distance functions
// from https://iquilezles.org/articles/distfunctions/
namespace DistFn
{
	inline float Sphere( const FGPUSDF& SDF, const FVector3f& Pos )
	{
		return (Pos - SDF.Pos).Size() - SDF.GetSphereRadius();
	}

	inline float Line( const FGPUSDF& SDF, const FVector3f& Pos )
	{
		FVector3f PA = Pos - SDF.Pos;
		FVector3f BA = SDF.GetLinePosB();
		float H = FMath::Clamp(FVector3f::DotProduct(PA, BA) / FVector3f::DotProduct(BA, BA), 0.0, 1.0);
		return (PA - BA * H).Size() - SDF.GetLineRadius();
	}

	inline float Box( const FGPUSDF& SDF, const FVector3f& Pos )
	{
		FVector3f HalfExtent = SDF.GetBoxHalfExtent();
		FVector3f Q = (Pos - SDF.Pos).GetAbs() - HalfExtent;
		return Q.ComponentMax(FVector3f::ZeroVector).Size()
			+ FMath::Min(FMath::Max3(Q.X, Q.Y, Q.Z), 0.0f);
	}

	inline float SphereOrigin( const FGPUSDF& SDF, const FVector3f& Pos )
	{
		return Pos.Size() - SDF.GetSphereRadius();
	}

	inline float LineOrigin( const FGPUSDF& SDF, const FVector3f& Pos )
	{
		FVector3f PA = Pos;
		FVector3f BA = SDF.GetLinePosB();
		float H = FMath::Clamp(FVector3f::DotProduct(PA, BA) / FVector3f::DotProduct(BA, BA), 0.0, 1.0);
		return (PA - BA * H).Size() - SDF.GetLineRadius();
	}

	inline float BoxOrigin( const FGPUSDF& SDF, const FVector3f& Pos )
	{
		FVector3f HalfExtent = SDF.GetBoxHalfExtent();
		FVector3f Q = Pos.GetAbs() - HalfExtent;
		return Q.ComponentMax(FVector3f::ZeroVector).Size() 
			+ FMath::Min(FMath::Max3(Q.X, Q.Y, Q.Z), 0.0f);
	}

	inline float Dist( const FGPUSDF& SDF, const FVector3f& Pos )
	{
		EPrimitive Prim = SDF.GetPrimitive();
		if (Prim == EPrimitive::Sphere) { return Sphere(SDF, Pos); }
		if (Prim == EPrimitive::Line) { return Line(SDF, Pos); }
		return Box(SDF, Pos);
	}

	inline float DistOrigin( const FGPUSDF& SDF, const FVector3f& Pos )
	{
		EPrimitive Prim = SDF.GetPrimitive();
		if (Prim == EPrimitive::Sphere) { return SphereOrigin(SDF, Pos); }
		else if (Prim == EPrimitive::Line) { return LineOrigin(SDF, Pos); }
		return BoxOrigin(SDF, Pos);
	}
} // SDF

struct FAABB
{
	FVector3f Min;
	FVector3f Max;
};

// Bounding boxes
// From https://iquilezles.org/articles/bboxes3d/
namespace AABB
{
	inline FAABB Sphere( const FGPUSDF& SDF )
	{
		const float Radius = SDF.GetSphereRadius();

		FAABB Bounds;
		Bounds.Min = SDF.Pos - Radius;
		Bounds.Max = SDF.Pos + Radius;
		return Bounds;
	}

	inline FAABB Line( const FGPUSDF& SDF )
	{
		const FVector3f PosB = SDF.GetLinePosB();
		const float Radius = SDF.GetLineRadius();

		FAABB Bounds;
		Bounds.Min = SDF.Pos.ComponentMin(SDF.Pos + PosB) - Radius;
		Bounds.Max = SDF.Pos.ComponentMax(SDF.Pos + PosB) + Radius;
		return Bounds;
	}

	inline FAABB Box( const FGPUSDF& SDF )
	{
		const FVector3f HalfExtent = SDF.GetBoxHalfExtent();

		FAABB Bounds;
		Bounds.Min = SDF.Pos - HalfExtent;
		Bounds.Max = SDF.Pos + HalfExtent;
		return Bounds;
	}

	inline FAABB GetSDFAABB( const FGPUSDF& SDF )
	{
		EPrimitive Prim = SDF.GetPrimitive();
		if ( Prim == EPrimitive::Sphere ) { return Sphere( SDF ); }
		else if ( Prim == EPrimitive::Line ) { return Line( SDF );}
		return Box( SDF );
	}
} // AABB