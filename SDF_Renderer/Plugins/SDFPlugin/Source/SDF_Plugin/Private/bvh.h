#pragma once

struct FGPUSDF;
struct FSDFBounds;
class FSDFBVH
{
public:

	struct FRay
	{
		FRay( const FVector3f& Pos, const FVector3f& Dir )
			: Pos{ Pos }
			, Dir{ Dir }
			// TODO: prevent NaN?
			, InvDir{ FVector3f::OneVector / Dir }
		{}

		bool Hit() const { return T != MAX_T; }

		FVector3f Pos;
		FVector3f Dir;
		FVector3f InvDir;
		float T{ MAX_T };
	};

	struct FHitResult
	{
		uint32 EditIdx;
		float T;
		bool bHit;
	};

	struct alignas( 32 ) FNode
	{
		float Intersect( const FRay& Ray ) const;
		bool IsLeaf() const { return NumEdits > 0; }

		FVector3f BoundsMin;
		uint32 LeftFirst;
		FVector3f BoundsMax;
		uint32 NumEdits;
	};
	static_assert( sizeof( FNode ) == 32 );

	~FSDFBVH();

	FHitResult Intersect( FRay Ray, const TArray<FGPUSDF>& Edits ) const;
	void Build( uint32 NewNumEdits, const TArray<FSDFBounds>& EditsBounds );
	void Refit( const TArray<FSDFBounds>& EditsBounds );

private:

	static constexpr float MAX_T{ TNumericLimits<float>::Max() };
	static constexpr size_t NODE_STACK_CAPACITY{ 64ull };
	static constexpr uint32 ROOT_IDX{ 0u };

	void Subdivide( FNode& Node, const TArray<FSDFBounds>& EditsBounds );
	void RefitNode( FNode& Node, const TArray<FSDFBounds>& EditsBounds ) const;
	static uint32 NumNodesForNObjects( uint32 N ) { return 2 * N; }

	uint32* EditIndices{ nullptr };
	FNode* Nodes{ nullptr };
	uint32 NodesUsed{ 1u };
	uint32 NumEdits{ 0u };

}; // SDFBVH