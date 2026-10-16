#pragma once

struct FSDF;
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

	FSDFBVH() = default;
	~FSDFBVH();
	FSDFBVH( const FSDFBVH& Other );
	FSDFBVH& operator=( const FSDFBVH& Other );

	FHitResult Intersect( FRay Ray, const TMap<uint64, FSDF>& Edits ) const;
	void Build( const TMap<uint64, FSDF>& Edits );
	void Refit( const TMap<uint64, FSDF>& Edits );

	[[nodiscard]] const FNode* GetNodes() const { return Nodes; }
	[[nodiscard]] uint32 GetNumNodes() const { return NodesUsed; }
	void ScaleBVH( float UnitScale ) const;

private:

	static constexpr float MAX_T{ TNumericLimits<float>::Max() };
	static constexpr size_t NODE_STACK_CAPACITY{ 64ull };
	static constexpr uint32 ROOT_IDX{ 0u };

	void Subdivide( FNode& Node, const TMap<uint64, FSDF>& Edits );
	void RefitNode( FNode& Node, const TMap<uint64, FSDF>& Edits ) const;
	static uint32 NumNodesForNObjects( uint32 N ) { return FMath::Max( 2u * N, 2u ); }

	uint32* EditIndices{ nullptr };
	FNode* Nodes{ nullptr };
	uint32 NodesUsed{ 0u };
	uint32 NumEdits{ 0u };

}; // SDFBVH