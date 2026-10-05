// Copyright Demar Games. All Rights Reserved.

#include "DG_VoxelGrid.h"

void FDG_VoxelGrid::Reset(int32 InDimX, int32 InDimY, int32 InDimZ, float InVoxelSize, const FVector& InOrigin)
{
	DimX = InDimX;
	DimY = InDimY;
	DimZ = InDimZ;
	VoxelSize = InVoxelSize;
	Origin = InOrigin;
	const int32 N = NumBits();
	Occupancy.SetNum(N, false);
	Density.SetNumZeroed(N);
}

void FDG_VoxelGrid::SyncDensityFromOccupancy()
{
	const int32 N = NumBits();
	Density.SetNumUninitialized(N);
	for (int32 I = 0; I < N; ++I)
	{
		Density[I] = Occupancy[I] ? 1.f : 0.f;
	}
}

void FDG_VoxelGrid::SyncOccupancyFromDensity()
{
	const int32 N = NumBits();
	Occupancy.SetNum(N, false);
	for (int32 I = 0; I < N; ++I)
	{
		Occupancy[I] = Density[I] >= 0.5f;
	}
}

void FDG_VoxelGrid::DilateSolidOnce()
{
	const int32 N = NumBits();
	if (N <= 0)
	{
		return;
	}

	const FVoxelBitArray Src = Occupancy;
	for (int32 Z = 0; Z < DimZ; ++Z)
	{
		for (int32 Y = 0; Y < DimY; ++Y)
		{
			for (int32 X = 0; X < DimX; ++X)
			{
				if (!Src[IndexOf(X, Y, Z)])
				{
					continue;
				}
				SetSolid(X + 1, Y, Z, true);
				SetSolid(X - 1, Y, Z, true);
				SetSolid(X, Y + 1, Z, true);
				SetSolid(X, Y - 1, Z, true);
				SetSolid(X, Y, Z + 1, true);
				SetSolid(X, Y, Z - 1, true);
			}
		}
	}
}

void FDG_VoxelGrid::FillInteriorFromSurface()
{
	const int32 N = NumBits();
	if (N <= 0)
	{
		return;
	}

	FVoxelBitArray Outside;
	Outside.SetNum(N, false);
	TArray<FIntVector> Stack;
	Stack.Reserve(N / 8 + 16);

	auto TryPush = [&](int32 X, int32 Y, int32 Z)
	{
		if (!InBounds(X, Y, Z))
		{
			return;
		}
		const int32 I = IndexOf(X, Y, Z);
		if (Occupancy[I] || Outside[I])
		{
			return;
		}
		Outside[I] = true;
		Stack.Add(FIntVector(X, Y, Z));
	};

	for (int32 Z = 0; Z < DimZ; ++Z)
	{
		for (int32 Y = 0; Y < DimY; ++Y)
		{
			TryPush(0, Y, Z);
			TryPush(DimX - 1, Y, Z);
		}
	}
	for (int32 Z = 0; Z < DimZ; ++Z)
	{
		for (int32 X = 0; X < DimX; ++X)
		{
			TryPush(X, 0, Z);
			TryPush(X, DimY - 1, Z);
		}
	}
	for (int32 Y = 0; Y < DimY; ++Y)
	{
		for (int32 X = 0; X < DimX; ++X)
		{
			TryPush(X, Y, 0);
			TryPush(X, Y, DimZ - 1);
		}
	}

	while (Stack.Num() > 0)
	{
		const FIntVector V = Stack.Pop(EAllowShrinking::No);
		TryPush(V.X + 1, V.Y, V.Z);
		TryPush(V.X - 1, V.Y, V.Z);
		TryPush(V.X, V.Y + 1, V.Z);
		TryPush(V.X, V.Y - 1, V.Z);
		TryPush(V.X, V.Y, V.Z + 1);
		TryPush(V.X, V.Y, V.Z - 1);
	}

	for (int32 I = 0; I < N; ++I)
	{
		Occupancy[I] = !Outside[I];
	}
	SyncDensityFromOccupancy();
}

bool FDG_VoxelGrid::FloodAndMaybeCull(
	int32 Start,
	FVoxelBitArray& Visited,
	TArray<int32>& Cluster,
	int32 MinClusterVoxels,
	FVoxelOptionalIntBox& OutDirty)
{
	Cluster.Reset();
	TArray<int32> Stack;
	Stack.Reserve(64);
	Stack.Add(Start);
	Visited[Start] = true;
	bool bLarge = false;

	while (Stack.Num() > 0)
	{
		const int32 I = Stack.Pop(EAllowShrinking::No);
		if (!bLarge)
		{
			Cluster.Add(I);
			if (Cluster.Num() >= MinClusterVoxels)
			{
				bLarge = true;
				Cluster.Reset();
			}
		}

		const int32 X = I % DimX;
		const int32 Y = (I / DimX) % DimY;
		const int32 Z = I / (DimX * DimY);

		auto PushN = [&](int32 NX, int32 NY, int32 NZ)
		{
			if (!InBounds(NX, NY, NZ))
			{
				return;
			}
			const int32 NI = IndexOf(NX, NY, NZ);
			if (!Occupancy[NI] || Visited[NI])
			{
				return;
			}
			Visited[NI] = true;
			Stack.Add(NI);
		};

		PushN(X + 1, Y, Z);
		PushN(X - 1, Y, Z);
		PushN(X, Y + 1, Z);
		PushN(X, Y - 1, Z);
		PushN(X, Y, Z + 1);
		PushN(X, Y, Z - 1);
	}

	if (bLarge)
	{
		return false;
	}

	for (const int32 I : Cluster)
	{
		ClearVoxel(I);
		OutDirty += FIntVector(I % DimX, (I / DimX) % DimY, I / (DimX * DimY));
	}
	return true;
}

void FDG_VoxelGrid::RemoveSmallClusters(int32 MinClusterVoxels, FVoxelOptionalIntBox& OutDirty)
{
	const int32 N = NumBits();
	if (N <= 0 || MinClusterVoxels <= 1)
	{
		return;
	}

	FVoxelBitArray Visited;
	Visited.SetNum(N, false);
	TArray<int32> Cluster;
	Cluster.Reserve(MinClusterVoxels);

	for (int32 Start = 0; Start < N; ++Start)
	{
		if (Occupancy[Start] && !Visited[Start])
		{
			FloodAndMaybeCull(Start, Visited, Cluster, MinClusterVoxels, OutDirty);
		}
	}
}

int32 FDG_VoxelGrid::CountSolidFalloff(
	const FVector& CenterLocal,
	float Radius,
	const FVoxelFalloff& Falloff,
	float CarveThreshold) const
{
	if (Radius <= 0.f || NumBits() <= 0)
	{
		return 0;
	}

	const float Threshold = FMath::Clamp(CarveThreshold, 0.f, 1.f);
	const float RadiusSq = Radius * Radius;
	int32 X0, Y0, Z0, X1, Y1, Z1;
	GetRadiusVoxelBounds(CenterLocal, Radius, X0, Y0, Z0, X1, Y1, Z1);

	int32 Count = 0;
	for (int32 Z = Z0; Z <= Z1; ++Z)
	{
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			for (int32 X = X0; X <= X1; ++X)
			{
				const int32 I = IndexOf(X, Y, Z);
				if (Density[I] < 0.5f)
				{
					continue;
				}
				const float DistSq = FVector::DistSquared(VoxelCenterLocal(X, Y, Z), CenterLocal);
				if (DistSq > RadiusSq)
				{
					continue;
				}
				const float Dist = FMath::Sqrt(DistSq);
				if (FVoxelFalloff::GetFalloff(Falloff.Type, Dist, Radius, Falloff.Amount) >= Threshold)
				{
					++Count;
				}
			}
		}
	}
	return Count;
}

void FDG_VoxelGrid::GetRadiusVoxelBounds(
	const FVector& CenterLocal,
	float Radius,
	int32& OutX0, int32& OutY0, int32& OutZ0,
	int32& OutX1, int32& OutY1, int32& OutZ1) const
{
	const float InvVS = 1.f / VoxelSize;
	OutX0 = FMath::Clamp(FMath::FloorToInt((CenterLocal.X - Radius - Origin.X) * InvVS), 0, DimX - 1);
	OutY0 = FMath::Clamp(FMath::FloorToInt((CenterLocal.Y - Radius - Origin.Y) * InvVS), 0, DimY - 1);
	OutZ0 = FMath::Clamp(FMath::FloorToInt((CenterLocal.Z - Radius - Origin.Z) * InvVS), 0, DimZ - 1);
	OutX1 = FMath::Clamp(FMath::FloorToInt((CenterLocal.X + Radius - Origin.X) * InvVS), 0, DimX - 1);
	OutY1 = FMath::Clamp(FMath::FloorToInt((CenterLocal.Y + Radius - Origin.Y) * InvVS), 0, DimY - 1);
	OutZ1 = FMath::Clamp(FMath::FloorToInt((CenterLocal.Z + Radius - Origin.Z) * InvVS), 0, DimZ - 1);
}

int32 FDG_VoxelGrid::CarveFalloffShell(
	const FVector& CenterLocal,
	float PrevR,
	float CurrR,
	const FVoxelFalloff& Falloff,
	TArray<FIntVector>& OutRemoved,
	FVoxelOptionalIntBox& OutDirty,
	float Strength)
{
	if (CurrR <= PrevR || NumBits() <= 0)
	{
		return 0;
	}

	const float CurrRSq = CurrR * CurrR;
	int32 X0, Y0, Z0, X1, Y1, Z1;
	GetRadiusVoxelBounds(CenterLocal, CurrR, X0, Y0, Z0, X1, Y1, Z1);

	int32 Removed = 0;
	for (int32 Z = Z0; Z <= Z1; ++Z)
	{
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			for (int32 X = X0; X <= X1; ++X)
			{
				const int32 I = IndexOf(X, Y, Z);
				float& D = Density[I];
				if (D <= 0.f)
				{
					continue;
				}

				const float DistSq = FVector::DistSquared(VoxelCenterLocal(X, Y, Z), CenterLocal);
				if (DistSq > CurrRSq)
				{
					continue;
				}

				const float Dist = FMath::Sqrt(DistSq);
				const float CurrW = FVoxelFalloff::GetFalloff(Falloff.Type, Dist, CurrR, Falloff.Amount);
				const float PrevW = PrevR <= 0.f
					? 0.f
					: FVoxelFalloff::GetFalloff(Falloff.Type, Dist, PrevR, Falloff.Amount);
				const float Delta = CurrW - PrevW;
				if (Delta <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				const float Old = D;
				D = FMath::Max(0.f, Old - Delta * Strength);
				if (D >= Old)
				{
					continue;
				}

				OutDirty += FIntVector(X, Y, Z);
				const bool WasSolid = Old >= 0.5f;
				const bool IsSolid = D >= 0.5f;
				Occupancy[I] = IsSolid;
				if (WasSolid && !IsSolid)
				{
					OutRemoved.Add(FIntVector(X, Y, Z));
					++Removed;
				}
			}
		}
	}
	return Removed;
}

bool FDG_VoxelGrid::UnpackBits(const uint8* Packed, int32 PackedBytes, int32 BitCount)
{
	if (!Packed || BitCount != NumBits() || PackedBytes < (BitCount + 7) / 8)
	{
		return false;
	}

	Occupancy.SetNum(BitCount, false);
	for (int32 I = 0; I < BitCount; ++I)
	{
		if (Packed[I >> 3] & (1 << (I & 7)))
		{
			Occupancy[I] = true;
		}
	}
	SyncDensityFromOccupancy();
	return true;
}

void FDG_VoxelGrid::PackDensity(TArray<uint8>& OutPacked) const
{
	const int32 Bytes = NumBits() * (int32)sizeof(float);
	OutPacked.SetNumUninitialized(Bytes);
	if (Bytes > 0)
	{
		FMemory::Memcpy(OutPacked.GetData(), Density.GetData(), Bytes);
	}
}

bool FDG_VoxelGrid::UnpackDensity(const uint8* Packed, int32 PackedBytes, int32 Count)
{
	const int32 Need = Count * (int32)sizeof(float);
	if (!Packed || Count != NumBits() || PackedBytes < Need)
	{
		return false;
	}

	Density.SetNumUninitialized(Count);
	FMemory::Memcpy(Density.GetData(), Packed, Need);
	SyncOccupancyFromDensity();
	return true;
}
