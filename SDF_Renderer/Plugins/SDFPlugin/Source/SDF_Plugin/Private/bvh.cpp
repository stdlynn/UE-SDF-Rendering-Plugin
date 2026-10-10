#include "bvh.h"

#include "SDFEdits.h"

void FSDFBVH::Build( const TArray<FGPUSDF>& Edits )
{
	// TODO: deallocate if NumEdits != Edits.Num()
	// TODO: allocate if deallocated and Edits.Num() > 0
	// TODO: - Including (de)allocating 'EditIndices'
	// TODO: return if Edits.Num() == 0

	if (Nodes)
	{
		// TODO: free64
		Nodes = nullptr;
	}
	if (Edits.Num())
	{
		// TODO: new64
	}

	if (not Edits.Num()) { return; }
	if (NumEdits)

	const FAABB* AABBs{}; // TODO

	FNode& Root{ GetRoot() };
	Root.LeftChildIdx = 0;
	Root.FirstEdit = 0;
	Root.NumEdits = NumEdits;

	// Update node bounds
	Root.Refit( AABBs );

	// Recursively subdivide
	Subdivide( Root );
}

void FSDFBVH::Subdivide( FNode& Node )
{
	// Stop subdividing once there's less than 2 edits left in a node
	if (Node.NumEdits <= 2) { return; }

	// Find the split plane axis and position
	const FVector3f Extent{ Node.BoundsMax - Node.BoundsMin };
	const uint32 Axis
	{
		[Extent]() -> uint32
		{
			uint32 Axis{ 0 };
			if (Extent.Y > Extent.X) { Axis = 1u; }
			if (Extent.Z > Extent[Axis]) { Axis = 2u; }
			return Axis;
		}()
	};
	const float SplitPos{ Node.BoundsMin[Axis] + (Extent[Axis] / 2.0f) };
	
	// Go over all primitives held by the node
	uint32 LeftMostToSort{ Node.FirstEdit };
	uint32 RightMostToSort{ LeftMostToSort + Node.NumEdits - 1 };
	// While there are primitives left to be sorted
	while (LeftMostToSort <= RightMostToSort)
	{
		// TODO: instead of storing that separate list of AABBs
		// TODO: also store other necessary stuff in there, like the centroids
		// Keep edits on the left side of the split plane at the start of the index buffer
		if (EditCentroid[Axis] < SplitPos) { LeftMostToSort++; }
		// Move edits on the right side of the split plane to the end of the index buffer
		else { Swap( EditIndices[LeftMostToSort], EditIndices[RightMostToSort--] ); }
	}

	// Stop subdivision if the left or right side of the split plane doesn't contain any edits
	const uint32 NumLeftEdits{ LeftMostToSort - Node.FirstEdit };
	if (NumLeftEdits == 0u || NumLeftEdits == Node.NumEdits) { return; }

	// Add child nodes
	const uint32 LeftChildIdx{ NodesUsed++ };
	const uint32 RightChildIdx{ NodesUsed++ };
	Node.LeftChildIdx = LeftChildIdx;
	FNode& LeftChild{ Nodes[LeftChildIdx] };
	FNode& RightChild{ Nodes[RightChildIdx] };

	// Assign the children their respective primitives
	LeftChild.FirstEdit = Node.FirstEdit;
	LeftChild.NumEdits = NumLeftEdits;
	RightChild.FirstEdit = LeftMostToSort;
	RightChild.NumEdits = Node.NumEdits - NumLeftEdits;
	// The original node is no longer a leaf
	Node.NumEdits = 0u;

	// Set the bounds for the child nodes
	LeftChild.Refit(  );
	RightChild.Refit(  );

	// Further subdivide both children
	Subdivide( LeftChild );
	Subdivide( RightChild );
}

void FSDFBVH::FNode::Refit( const FAABB* AABBs )
{
	BoundsMin = FVector3f{ TNumericLimits<float>::Min() };
	BoundsMax = FVector3f{ TNumericLimits<float>::Max() };
	for (uint32 i{ 0 }; i < NumEdits; ++i)
	{
		const FAABB& Bounds{ AABBs[FirstEdit + i] };
		BoundsMin = FMath::Min( BoundsMin, Bounds.Min );
		BoundsMax = FMath::Max( BoundsMax, Bounds.Max );
	}
}