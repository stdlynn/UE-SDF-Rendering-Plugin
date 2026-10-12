#pragma once

struct FGPUSDF;
struct FSDFBounds;
class FSDFBVH
{
public:

	~FSDFBVH();

	struct alignas( 32 ) FNode
	{
		float Intersect( const FRay3f& Ray, const FVector3f& InvDir ) const;
		bool IsLeaf() const { return NumEdits; }

		FVector3f BoundsMin;
		uint32 LeftFirst;
		FVector3f BoundsMax;
		uint32 NumEdits;
	};
	static_assert( sizeof( FNode ) == 32 );

	bool Intersect( const FRay3f& Ray, const TArray<FGPUSDF>& Edits ) const;
	void Build( const TArray<FGPUSDF>& Edits, const TArray<FSDFBounds>& EditsBounds );

private:

	static constexpr float MAX_T{ TNumericLimits<float>::Max() };
	static constexpr uint32 ROOT_IDX{ 0u };

	void Subdivide( FNode& Node, const TArray<FSDFBounds>& EditsBounds );
	void RefitNode( FNode& Node, const TArray<FSDFBounds>& EditsBounds ) const;
	static uint32 NumNodesForNObjects( uint32 N ) { return 2 * N; }

	uint32* EditIndices{ nullptr };
	FNode* Nodes{ nullptr };
	uint32 NodesUsed{ 1u };
	uint32 NumEdits{ 0u };

}; // SDFBVH