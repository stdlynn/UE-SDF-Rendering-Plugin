#pragma once

struct FGPUSDF;
struct FAABB;
class FSDFBVH
{
public:

	struct alignas( 32 ) Node
	{
		void Refit( const FAABB* AABBs );
		bool IsLeaf() const { return NumEdits; }

		FVector3f BoundsMin;
		uint32 LeftChild, RightChild;
		FVector3f BoundsMax;
		uint32 FirstEdit, NumEdits;
	};
	static_assert( sizeof( Node ) == 32 );

	void Build();

private:

	void Subdivide( uint32 NodeIdx );

	Node& GetRoot() { return Nodes[RootIdx]; }

	Node* Nodes{ nullptr };
	uint32 RootIdx{ 0u };
	uint32 NodesUsed{ 1u };

}; // SDFBVH