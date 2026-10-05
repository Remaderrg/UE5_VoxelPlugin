// Copyright Demar Games. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelFalloff.h"

/** Density + occupancy grid (VoxelCore). Not a UObject — keep out of UHT headers. */
struct FDG_VoxelGrid
{
	int32 DimX = 0;
	int32 DimY = 0;
	int32 DimZ = 0;
	float VoxelSize = 10.f;
	FVector Origin = FVector::ZeroVector;
	FVoxelBitArray Occupancy;
	TVoxelArray<float> Density;

	FORCEINLINE int32 NumBits() const { return DimX * DimY * DimZ; }
	FORCEINLINE int32 IndexOf(int32 X, int32 Y, int32 Z) const { return X + DimX * (Y + DimY * Z); }
	FORCEINLINE bool InBounds(int32 X, int32 Y, int32 Z) const
	{
		return (uint32)X < (uint32)DimX && (uint32)Y < (uint32)DimY && (uint32)Z < (uint32)DimZ;
	}
	FORCEINLINE float GetDensity(int32 X, int32 Y, int32 Z) const
	{
		return InBounds(X, Y, Z) ? Density[IndexOf(X, Y, Z)] : 0.f;
	}
	FORCEINLINE bool GetSolid(int32 X, int32 Y, int32 Z) const
	{
		return GetDensity(X, Y, Z) >= 0.5f;
	}
	FORCEINLINE void SetSolid(int32 X, int32 Y, int32 Z, bool bSolid)
	{
		if (!InBounds(X, Y, Z))
		{
			return;
		}
		const int32 I = IndexOf(X, Y, Z);
		Occupancy[I] = bSolid;
		Density[I] = bSolid ? 1.f : 0.f;
	}
	FORCEINLINE void AtomicSetSolid(int32 X, int32 Y, int32 Z, bool bSolid)
	{
		if (InBounds(X, Y, Z))
		{
			Occupancy.AtomicSet(IndexOf(X, Y, Z), bSolid);
		}
	}

	FVector VoxelCenterLocal(int32 X, int32 Y, int32 Z) const
	{
		return Origin + FVector((X + 0.5f) * VoxelSize, (Y + 0.5f) * VoxelSize, (Z + 0.5f) * VoxelSize);
	}

	void Reset(int32 InDimX, int32 InDimY, int32 InDimZ, float InVoxelSize, const FVector& InOrigin);
	void SyncDensityFromOccupancy();
	void DilateSolidOnce();
	void FillInteriorFromSurface();
	/** Cull solid groups smaller than MinClusterVoxels; OutDirty = cleared voxels. */
	void RemoveSmallClusters(int32 MinClusterVoxels, FVoxelOptionalIntBox& OutDirty);

	int32 CountSolidFalloff(
		const FVector& CenterLocal,
		float Radius,
		const FVoxelFalloff& Falloff,
		float CarveThreshold) const;

	/** Subtract falloff delta (CurrR − PrevR). OutRemoved = solid→air; OutDirty = any density change. */
	int32 CarveFalloffShell(
		const FVector& CenterLocal,
		float PrevR,
		float CurrR,
		const FVoxelFalloff& Falloff,
		float CarveThreshold,
		TArray<FIntVector>& OutRemoved,
		FVoxelOptionalIntBox& OutDirty);

	void PackBits(TArray<uint8>& OutPacked) const;
	bool UnpackBits(const uint8* Packed, int32 PackedBytes, int32 BitCount);

private:
	bool FloodAndMaybeCull(
		int32 Start,
		FVoxelBitArray& Visited,
		TArray<int32>& Cluster,
		int32 MinClusterVoxels,
		FVoxelOptionalIntBox& OutDirty);
	FORCEINLINE void ClearVoxel(int32 I)
	{
		Occupancy[I] = false;
		Density[I] = 0.f;
	}
};
