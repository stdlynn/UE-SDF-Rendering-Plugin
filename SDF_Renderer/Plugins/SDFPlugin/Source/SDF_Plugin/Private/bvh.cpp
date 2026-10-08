#include "bvh.h"

#include "SDFEdits.h"

void FSDFBVH::Build()
{
	uint32 NumEdits{};

	Node& Root{ GetRoot() };
	Root.LeftChild = 0;
	Root.RightChild = 0;
	Root.FirstEdit = 0;
	Root.NumEdits = NumEdits;

	// Commence recursive subdivision
	Subdivide( RootIdx );
}

void FSDFBVH::Subdivide( uint32 NodeIdx )
{
}

void FSDFBVH::Node::Refit( const FAABB* AABBs )
{
	BoundsMin = FVector3f{ TNumericLimits<float>::Min() };
	BoundsMin = FVector3f{ TNumericLimits<float>::Max() };
	for (uint32 i{ 0 }; i < NumEdits; ++i)
	{
		const FAABB& Bounds{ AABBs[FirstEdit + i] };
		BoundsMin = FMath::Min( BoundsMin, )
	}
}