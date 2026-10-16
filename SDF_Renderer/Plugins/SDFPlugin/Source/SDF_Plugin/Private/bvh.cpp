#include "bvh.h"

#include "SDFEdits.h"

FSDFBVH::~FSDFBVH()
{
	FMemory::Free( Nodes );
	delete[] EditIndices;
}

FSDFBVH::FHitResult FSDFBVH::Intersect( FRay Ray, const TMap<uint64, FSDF>& Edits ) const
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
				if (false /*Edits.Get( EditIdx )*/) // TODO: intersect SDFs (modify Ray.T)
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
	const FVector3f TMin = TToMin.ComponentMin( TToMax );
	const FVector3f TMax = TToMin.ComponentMax( TToMax );
	const float TNear = FMath::Max( FMath::Max3( TMin.X, TMin.Y, TMin.Z ), 0.0f );
	const float TFar = FMath::Min( FMath::Min3( TMax.X, TMax.Y, TMax.Z ), Ray.T );
	return (TNear <= TFar) ? TNear : MAX_T;
}

void FSDFBVH::Build( const TMap<uint64, FSDF>& Edits )
{
	NumEdits = Edits.Num();
	
	// Delete old resources
	FMemory::Free( Nodes );
	delete[] EditIndices;
	Nodes = nullptr;
	EditIndices = nullptr;
	
	// Allocate nodes
	Nodes = static_cast<FNode*>(FMemory::Malloc( NumNodesForNObjects( NumEdits ) * sizeof( FNode ), 64u ));
	NodesUsed = 2u; // Skip the 2nd node so children share a cacheline

	// Initialize root node
	FNode& Root{ Nodes[ROOT_IDX] };
	Root.LeftFirst = 0u;
	Root.NumEdits = NumEdits;

	// No need to subdivide if there's no edits
	if (not NumEdits) { return; }

	// Allocate and initialize the edit indices
	EditIndices = new uint32[NumEdits];
	for (uint32 i{ 0 }; i < NumEdits; ++i) { EditIndices[i] = i; }
	
	// Calculate the bounds of the BVH
	RefitNode( Root, Edits );

	// Recursively subdivide
	Subdivide( Root, Edits );
}

void FSDFBVH::Subdivide( FNode& Node, const TMap<uint64, FSDF>& Edits )
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
		const FSetElementId EditIdx{ FSetElementId::FromInteger( EditIndices[LeftMostToSort] ) };
		if (Edits.Get( EditIdx ).Value.Centroid[Axis] < SplitPos) { LeftMostToSort++; }
		// Move edits on the right side of the split plane to the end of the index buffer
		else
		{
			Swap( EditIndices[LeftMostToSort], EditIndices[RightMostToSort] );
			if (RightMostToSort == 0) { break; }
			RightMostToSort--;
		}
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
	RefitNode( LeftChild, Edits );
	RefitNode( RightChild, Edits );

	// Further subdivide both children
	Subdivide( LeftChild, Edits );
	Subdivide( RightChild, Edits );
}

void FSDFBVH::RefitNode( FNode& Node, const TMap<uint64, FSDF>& Edits ) const
{
	Node.BoundsMin = FVector3f{ TNumericLimits<float>::Max() };
	Node.BoundsMax = FVector3f{ TNumericLimits<float>::Lowest() };
	const uint32 EndIdx{ Node.LeftFirst + Node.NumEdits };
	for (uint32 i{ Node.LeftFirst }; i < EndIdx; ++i)
	{
		const FSetElementId BoundsIdx{ FSetElementId::FromInteger( EditIndices[i] ) };
		Node.BoundsMin = Node.BoundsMin.ComponentMin( Edits.Get( BoundsIdx ).Value.Bounds.Min );
		Node.BoundsMax = Node.BoundsMax.ComponentMax( Edits.Get( BoundsIdx ).Value.Bounds.Max );
	}
}

FSDFBVH::FSDFBVH( const FSDFBVH& Other )
	: NodesUsed{ Other.NodesUsed }
	, NumEdits{ Other.NumEdits }
{
	// Allocate and copy nodes
	if (NodesUsed)
	{
		const uint32 NumNodes{ NumNodesForNObjects( NumEdits ) };
		Nodes = static_cast<FNode*>(FMemory::Malloc( NumNodes * sizeof( FNode ), 64u ));
		FMemory::Memcpy( Nodes, Other.Nodes, NumNodes * sizeof( FNode ) );
	}

	// Allocate and copy edit indices (if applicable)
	if (NumEdits)
	{
		EditIndices = new uint32[NumEdits];
		FMemory::Memcpy( EditIndices, Other.EditIndices, NumEdits * sizeof( uint32 ) );
	}
}

FSDFBVH& FSDFBVH::operator=( const FSDFBVH& Other )
{
	NumEdits = Other.NumEdits;
	NodesUsed = Other.NodesUsed;

	// Free old resources
	FMemory::Free( Nodes );
	delete[] EditIndices;
	Nodes = nullptr;
	EditIndices = nullptr;

	// Allocate and copy nodes
	if (NodesUsed)
	{
		const uint32 NumNodes{ NumNodesForNObjects( NumEdits ) };
		Nodes = static_cast<FNode*>(FMemory::Malloc( NumNodes * sizeof( FNode ), 64u ));
		FMemory::Memcpy( Nodes, Other.Nodes, NumNodes * sizeof( FNode ) );
	}

	// Allocate and copy edit indices (if applicable)
	if (NumEdits)
	{
		EditIndices = new uint32[NumEdits];
		FMemory::Memcpy( EditIndices, Other.EditIndices, NumEdits * sizeof( uint32 ) );
	}

	return *this;
}

void FSDFBVH::ScaleBVH( float UnitScale ) const
{
	if (Nodes)
	{
		const uint32 NumNodes{ NumNodesForNObjects( NumEdits ) };
		for (uint32 i{ 0 }; i < NumNodes; ++i)
		{
			Nodes[i].BoundsMin *= UnitScale;
			Nodes[i].BoundsMax *= UnitScale;
		}
	}
}