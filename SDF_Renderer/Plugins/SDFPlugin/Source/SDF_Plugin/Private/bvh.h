#pragma once

struct FGPUSDF;
struct FAABB;
class FSDFBVH
{
public:

	struct alignas( 32 ) FNode
	{
		void Refit( const FAABB* AABBs );
		bool IsLeaf() const { return NumEdits; }

		FVector3f BoundsMin;
		uint32 LeftChildIdx;
		FVector3f BoundsMax;
		uint32 FirstEdit, NumEdits;
	};
	static_assert( sizeof( FNode ) == 32 );

	void Build( const TArray<FGPUSDF>& Edits );

private:

	static constexpr uint32 ROOT_IDX{ 0u };

	void Subdivide( FNode& Node );
	FNode& GetRoot() { return Nodes[ROOT_IDX]; }

	uint32* EditIndices{ nullptr };
	FNode* Nodes{ nullptr };
	uint32 NodesUsed{ 1u };
	uint32 NumEdits{ 0u };

}; // SDFBVH