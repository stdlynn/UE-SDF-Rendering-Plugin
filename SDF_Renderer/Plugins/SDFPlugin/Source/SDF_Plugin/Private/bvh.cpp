#include "bvh.h"

#include "SDFEdits.h"

FSDFBVH::~FSDFBVH()
{
	FMemory::Free( Nodes );
	delete[] EditIndices;
}

FSDFBVH::FHitResult FSDFBVH::Intersect( FRay Ray, const TArray<FGPUSDF>& Edits ) const
{
	FHitResult HitResult{ TNumericLimits<uint32>::Max(), MAX_T, false };

	// Don't do intersection tests if there are no primitives
	if (not NumEdits) { return HitResult; }

	// Keep a stack of notes to be processed
	uint32 NodeIdx{ ROOT_IDX };
	uint32 NodeIdxStack[NODE_STACK_CAPACITY];
	uint32 StackPtr{ 0u };
	while (true)
	{
		// Grab the node to process
		const FNode& Node{ Nodes[NodeIdx] };

		// If the node has no children
		if (Node.IsLeaf())
		{
			// Go over the node's edits
			const uint32 EndIdx{ Node.LeftFirst + Node.NumEdits };
			for (uint32 i{ Node.LeftFirst }; i < EndIdx; ++i)
			{
				// Intersect
				const uint32 EditIdx{ EditIndices[i] };
				if (Edits[EditIdx]) // TODO: intersect SDFs (modify Ray.T)
				{
					HitResult.EditIdx = EditIdx;
					HitResult.T = Ray.T;
					HitResult.bHit = true;
				}
			}
		}
		// If the node has children
		else
		{
			// Retrieve the distance to the children
			uint32 LeftChildIdx{ Node.LeftFirst };
			uint32 RightChildIdx{ Node.LeftFirst + 1u };
			float LeftT{ Nodes[LeftChildIdx].Intersect( Ray ) };
			float RightT{ Nodes[RightChildIdx].Intersect( Ray ) };
			
			// Order the children by intersection distance
			if (LeftT > RightT)
			{
				Swap( LeftT, RightT );
				Swap( LeftChildIdx, RightChildIdx );
			}

			// If the ray intersects at least one child
			if (LeftT < MAX_T)
			{
				// Process said child next
				NodeIdx = LeftChildIdx;

				// If the ray also intersects the other child,
				// put it in the queue for processing
				if (RightT < MAX_T)
				{
					NodeIdxStack[StackPtr++] = RightChildIdx;
				}

				// Don't advance the stack pointer
				continue;
			}
		}

		// Break if there are no more nodes to process
		if (StackPtr == 0u) { break; }
		// Otherwise, continue with the next node on the stack
		NodeIdx = NodeIdxStack[--StackPtr];
	}

	return HitResult;
}

float FSDFBVH::FNode::Intersect( const FRay& Ray ) const
{
	const FVector3f TToMin = (BoundsMin - Ray.Pos) * Ray.InvDir;
	const FVector3f TToMax = (BoundsMax - Ray.Pos) * Ray.InvDir;
	const FVector3f TMin = FMath::Min( TToMin, TToMax );
	const FVector3f TMax = FMath::Max( TToMin, TToMax );
	const float TNear = FMath::Max( FMath::Max3( TMin.X, TMin.Y, TMin.Z ), 0.0f );
	const float TFar = FMath::Min( FMath::Min3( TMax.X, TMax.Y, TMax.Z ), Ray.T );
	return (TNear <= TFar) ? TNear : MAX_T;
}

void FSDFBVH::Build( uint32 NewNumEdits, const TArray<FSDFBounds>& EditsBounds )
{
	// Reset resources
	const bool bRealloc{ NumEdits != NewNumEdits };
	if (bRealloc)
	{
		FMemory::Free( Nodes );
		delete[] EditIndices;
		Nodes = nullptr;
		EditIndices = nullptr;
		NumEdits = NewNumEdits;
	}
	if (not NumEdits) { return; }
	if (bRealloc)
	{
		Nodes = static_cast<FNode*>(FMemory::Malloc( NumNodesForNObjects( NumEdits ) * sizeof( FNode ), 64u ));
		EditIndices = new uint32[NumEdits];
	}
	for (uint32 i{ 0 }; i < NumEdits; ++i) { EditIndices[i] = i; }

	// Initialize root node
	FNode& Root{ Nodes[ROOT_IDX] };
	Root.LeftFirst = 0u;
	Root.NumEdits = NumEdits;
	NodesUsed = 2u; // Skip the 2nd node so children share a cacheline
	RefitNode( Root, EditsBounds );

	// Recursively subdivide
	Subdivide( Root, EditsBounds );
}

void FSDFBVH::Subdivide( FNode& Node, const TArray<FSDFBounds>& EditsBounds )
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
	uint32 LeftMostToSort{ Node.LeftFirst };
	uint32 RightMostToSort{ LeftMostToSort + Node.NumEdits - 1 };
	// While there are primitives left to be sorted
	while (LeftMostToSort <= RightMostToSort)
	{
		// Keep edits on the left side of the split plane at the start of the index buffer
		if (EditsBounds[EditIndices[LeftMostToSort]].Centroid[Axis] < SplitPos) { LeftMostToSort++; }
		// Move edits on the right side of the split plane to the end of the index buffer
		else { Swap( EditIndices[LeftMostToSort], EditIndices[RightMostToSort--] ); }
	}

	// Stop subdivision if the left or right side of the split plane doesn't contain any edits
	const uint32 NumLeftEdits{ LeftMostToSort - Node.LeftFirst };
	if (NumLeftEdits == 0u || NumLeftEdits == Node.NumEdits) { return; }

	// Add child nodes
	const uint32 LeftChildIdx{ NodesUsed++ };
	const uint32 RightChildIdx{ NodesUsed++ };
	FNode& LeftChild{ Nodes[LeftChildIdx] };
	FNode& RightChild{ Nodes[RightChildIdx] };

	// Assign the children their respective primitives
	LeftChild.LeftFirst = Node.LeftFirst;
	LeftChild.NumEdits = NumLeftEdits;
	RightChild.LeftFirst = LeftMostToSort;
	RightChild.NumEdits = Node.NumEdits - NumLeftEdits;
	// The original node is no longer a leaf
	Node.LeftFirst = LeftChildIdx;
	Node.NumEdits = 0u;

	// Set the bounds for the child nodes
	RefitNode( LeftChild, EditsBounds );
	RefitNode( RightChild, EditsBounds );

	// Further subdivide both children
	Subdivide( LeftChild, EditsBounds );
	Subdivide( RightChild, EditsBounds );
}

void FSDFBVH::RefitNode( FNode& Node, const TArray<FSDFBounds>& EditsBounds ) const
{
	Node.BoundsMin = FVector3f{ TNumericLimits<float>::Lowest() };
	Node.BoundsMax = FVector3f{ TNumericLimits<float>::Max() };
	const uint32 EndIdx{ Node.LeftFirst + Node.NumEdits };
	for (uint32 i{ Node.LeftFirst }; i < EndIdx; ++i)
	{
		const uint32 BoundsIdx{ EditIndices[i] };
		const FSDFBounds& Bounds{ EditsBounds[BoundsIdx] };
		Node.BoundsMin = FMath::Min( Node.BoundsMin, Bounds.AABB.Min );
		Node.BoundsMax = FMath::Max( Node.BoundsMax, Bounds.AABB.Max );
	}
}