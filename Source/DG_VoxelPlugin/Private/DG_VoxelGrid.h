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

	FORCEINLINE FVector ClosestOnVoxel(const FVector& P, int32 X, int32 Y, int32 Z) const
	{
		const FVector C = VoxelCenterLocal(X, Y, Z);
		const float H = VoxelSize * 0.5f;
		return FVector(
			FMath::Clamp(P.X, C.X - H, C.X + H),
			FMath::Clamp(P.Y, C.Y - H, C.Y + H),
			FMath::Clamp(P.Z, C.Z - H, C.Z + H));
	}

	/** Squared distance from P to voxel AABB (0 if P is inside the cell). */
	FORCEINLINE float VoxelDistSq(const FVector& P, int32 X, int32 Y, int32 Z) const
	{
		return (ClosestOnVoxel(P, X, Y, Z) - P).SizeSquared();
	}

	void Reset(int32 InDimX, int32 InDimY, int32 InDimZ, float InVoxelSize, const FVector& InOrigin);
	void SyncDensityFromOccupancy();
	void SyncOccupancyFromDensity();
	void DilateSolidOnce();
	void FillInteriorFromSurface();
	/** Cull solid groups smaller than MinClusterVoxels; OutDirty = cleared voxels. */
	void RemoveSmallClusters(int32 MinClusterVoxels, FVoxelOptionalIntBox& OutDirty);

	FORCEINLINE int32 CountSolid() const { return Occupancy.CountSetBits(); }

	/** Solid voxels whose AABB overlaps the sphere. */
	int32 CountSolidInRadius(const FVector& CenterLocal, float Radius) const;

	/** Move Center onto nearest solid AABB within SearchR (no-op if already overlapping). */
	bool SnapCenterToSolid(FVector& InOutCenter, float SearchR) const;

	/** Subtract falloff delta (CurrR − PrevR). Aniso = ellipsoid; EdgeNoise = rim warp (0..~0.25 of R). */
	int32 CarveFalloffShell(
		const FVector& CenterLocal,
		float PrevR,
		float CurrR,
		const FVoxelFalloff& Falloff,
		TArray<FIntVector>& OutRemoved,
		FVoxelOptionalIntBox& OutDirty,
		float Strength = 1.f,
		FVector AnisoScale = FVector::OneVector,
		FQuat AnisoRot = FQuat::Identity,
		float EdgeNoise = 0.f);

	bool UnpackBits(const uint8* Packed, int32 PackedBytes, int32 BitCount);
	void PackDensity(TArray<uint8>& OutPacked) const;
	bool UnpackDensity(const uint8* Packed, int32 PackedBytes, int32 Count);

private:
	void GetRadiusVoxelBounds(
		const FVector& CenterLocal,
		float Radius,
		int32& OutX0, int32& OutY0, int32& OutZ0,
		int32& OutX1, int32& OutY1, int32& OutZ1) const;

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
