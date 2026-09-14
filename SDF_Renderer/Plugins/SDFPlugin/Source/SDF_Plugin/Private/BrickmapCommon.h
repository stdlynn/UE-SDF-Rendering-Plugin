#pragma once

// UE
#include "CoreMinimal.h"

static constexpr uint32 BrickResolution = 8;
static constexpr uint32 VoxelResolution = BrickResolution - 1;

static const FUintVector3 NumAtlasBricks = FUintVector3(128, 128, 1);
static const FUintVector3 AtlasSize = NumAtlasBricks * BrickResolution;

static const FUintVector3 PtrGridSize = FUintVector3(64, 64, 64);

// TODO move to separate SDFCommon.h file
inline float SDFSphere(const FVector3f& Point, const FVector3f& Center, float Radius)
{
	FVector3f Diff = Point - Center;
	return Diff.Size() - Radius;
}

inline FUintVector3 WorldPosToGridCell(const FVector3f& World)
{
    const FVector3f Scaled = World / VoxelResolution;
    return FUintVector3(FMath::FloorToInt(Scaled.X), FMath::FloorToInt(Scaled.Y), FMath::FloorToInt(Scaled.Z));
}

inline uint32 GridCellToGridIdx(const FUintVector3& GridCell)
{
	return GridCell.X + (GridCell.Y * PtrGridSize.X) + (GridCell.Z * PtrGridSize.Y * PtrGridSize.X);
}

inline FUintVector3 GridIdxToGridCell(uint32 Idx)
{
    const uint32 X = Idx % PtrGridSize.X;
    const uint32 Y = (Idx / PtrGridSize.X) % PtrGridSize.Y;
    const uint32 Z = Idx / (PtrGridSize.X * PtrGridSize.Y);
    return FUintVector3(X, Y, Z);
}

inline FVector3f GridCellToWorldPos(const FUintVector3& GridCell)
{ 
	return FVector3f(GridCell.X * VoxelResolution, GridCell.Y * VoxelResolution, GridCell.Z * VoxelResolution);
}