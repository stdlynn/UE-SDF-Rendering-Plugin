#pragma once

// UE
#include "CoreMinimal.h"

static constexpr uint32 BrickResolution = 8;
static constexpr uint32 VoxelResolution = BrickResolution - 1;

static const FUintVector3 NumAtlasBricks = FUintVector3( 256, 256, 1 );
static const FUintVector3 AtlasSize = NumAtlasBricks * BrickResolution;

static const FUintVector3 PtrGridSize = FUintVector3( 256, 256, 256 );

// TODO move to separate SDFCommon.h file
inline float SDFSphere( const FVector3f& Point, const FVector3f& Center, float Radius )
{
	const FVector3f Diff = Point - Center;
	return Diff.Size() - Radius;
}

inline float GetBrickSize( float VoxSize )
{
	return VoxSize * VoxelResolution;
}

inline FVector3f GetGridMin( const FVector3f& GridPos, float BrickSize )
{
	const FVector3f HalfExtent = FVector3f{ PtrGridSize } * BrickSize / 2.0;
	return GridPos - HalfExtent;
}

inline FVector3f GetGridMax( const FVector3f& GridPos, float BrickSize )
{
	const FVector3f HalfExtent = FVector3f{ PtrGridSize } * BrickSize / 2.0;
	return GridPos + HalfExtent;
}

// TODO: unused
inline FVector3f GridCellToWorldPos( const FIntVector3& GridCell, const FVector3f& GridMin, float BrickSize )
{
	return (FVector3f{ GridCell } * BrickSize) + GridMin;
}

inline FIntVector3 WorldPosToGridCell( const FVector3f& WorldPos, const FVector3f& GridMin, float BrickSize )
{
	const FVector3f Scaled = (WorldPos - GridMin) / BrickSize;
	return FIntVector3(
		FMath::FloorToInt( Scaled.X ),
		FMath::FloorToInt( Scaled.Y ),
		FMath::FloorToInt( Scaled.Z ) );
};

inline uint32 GridCellToGridIdx( const FUintVector3& GridCell )
{
	return GridCell.X + (GridCell.Y * PtrGridSize.X) + (GridCell.Z * PtrGridSize.Y * PtrGridSize.X);
}

// TODO: unused
inline FUintVector3 GridIdxToGridCell( uint32 GridIdx )
{
	const uint32 Z = GridIdx / (PtrGridSize.X * PtrGridSize.Y);
	const uint32 Intermediate = GridIdx - (Z * PtrGridSize.X * PtrGridSize.Y);
	const uint32 Y = Intermediate / PtrGridSize.X;
	const uint32 X = Intermediate - (Y * PtrGridSize.X);
	return FUintVector3( X, Y, Z );
}