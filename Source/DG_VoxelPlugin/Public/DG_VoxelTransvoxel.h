// Copyright Demar Games. All Rights Reserved.
//
// Transvoxel LUT data is part of Eric Lengyel's Transvoxel Algorithm (http://transvoxel.org/)
// as packaged by VoxelCore. See VoxelCore/Source/VoxelCore/Public/TransvoxelData.h for license.

#pragma once

#include "TransvoxelData.h"
#include "ProceduralMeshComponent.h"

namespace DGVoxelTV
{
	inline constexpr float IsoLevel = 0.5f;

	FORCEINLINE uint64 PackEdgeKey(int32 Axis, int32 GX, int32 GY, int32 GZ)
	{
		return (uint64(uint8(Axis)) << 48)
			| (uint64(uint16(GX)) << 32)
			| (uint64(uint16(GY)) << 16)
			| uint64(uint16(GZ));
	}

	/** Emit Transvoxel cell into chunk buffers. GetDensity(X,Y,Z) → 0..1 (1 solid). */
	template <typename GetDensity>
	static void EmitChunk(
		int32 CellX0, int32 CellY0, int32 CellZ0,
		int32 CellX1, int32 CellY1, int32 CellZ1,
		float VoxelSize, const FVector& SampleOrigin,
		GetDensity&& DensityFn,
		TArray<FVector>& Verts, TArray<int32>& Tris,
		TArray<FVector>& Normals, TArray<FVector2D>& UV0,
		TArray<FProcMeshTangent>& Tangents)
	{
		TVoxelMap<uint64, int32> EdgeVerts;
		EdgeVerts.Reserve(512);

		const FVector UVOrigin = SampleOrigin - FVector(VoxelSize * 0.5f);
		constexpr float InvCmPerUV = 0.01f;

		auto CornerDensity = [&](int32 CX, int32 CY, int32 CZ, int32 Corner) -> float
		{
			return DensityFn(CX + (Corner & 1), CY + ((Corner >> 1) & 1), CZ + ((Corner >> 2) & 1));
		};

		auto CornerPos = [&](int32 CX, int32 CY, int32 CZ, int32 Corner) -> FVector
		{
			return SampleOrigin + FVector(
				(CX + (Corner & 1)) * VoxelSize,
				(CY + ((Corner >> 1) & 1)) * VoxelSize,
				(CZ + ((Corner >> 2) & 1)) * VoxelSize);
		};

		// solid=1 → outward (to air) = -grad; stencil around nearest voxel
		auto GradientNormal = [&](float FX, float FY, float FZ) -> FVector
		{
			const int32 IX = FMath::RoundToInt(FX);
			const int32 IY = FMath::RoundToInt(FY);
			const int32 IZ = FMath::RoundToInt(FZ);
			FVector N(
				DensityFn(IX - 1, IY, IZ) - DensityFn(IX + 1, IY, IZ),
				DensityFn(IX, IY - 1, IZ) - DensityFn(IX, IY + 1, IZ),
				DensityFn(IX, IY, IZ - 1) - DensityFn(IX, IY, IZ + 1));
			if (!N.Normalize())
			{
				N = FVector::UpVector;
			}
			return N;
		};

		for (int32 Z = CellZ0; Z < CellZ1; ++Z)
		{
			for (int32 Y = CellY0; Y < CellY1; ++Y)
			{
				for (int32 X = CellX0; X < CellX1; ++X)
				{
					int32 CellCode = 0;
					float Val[8];
					for (int32 C = 0; C < 8; ++C)
					{
						Val[C] = CornerDensity(X, Y, Z, C);
						if (Val[C] < IsoLevel)
						{
							CellCode |= (1 << C);
						}
					}

					const Voxel::Transvoxel::FCellVertices& CellVerts =
						Voxel::Transvoxel::CellCodeToCellVertices[CellCode];
					const int32 NumCellVerts = CellVerts.NumVertices();
					if (NumCellVerts == 0)
					{
						continue;
					}

					int32 LocalToGlobal[12];
					for (int32 VI = 0; VI < NumCellVerts; ++VI)
					{
						const Voxel::Transvoxel::FVertexData VD = CellVerts.GetVertexData(VI);
						const int32 GX = X + (VD.IndexA & 1);
						const int32 GY = Y + ((VD.IndexA >> 1) & 1);
						const int32 GZ = Z + ((VD.IndexA >> 2) & 1);
						const uint64 Key = PackEdgeKey(VD.EdgeIndex, GX, GY, GZ);

						if (const int32* Existing = EdgeVerts.Find(Key))
						{
							LocalToGlobal[VI] = *Existing;
							continue;
						}

						const float Va = Val[VD.IndexA];
						const float Vb = Val[VD.IndexB];
						const float Denom = Vb - Va;
						const float T = FMath::Abs(Denom) > KINDA_SMALL_NUMBER
							? FMath::Clamp((IsoLevel - Va) / Denom, 0.f, 1.f)
							: 0.5f;

						const int32 AX = VD.IndexA & 1;
						const int32 AY = (VD.IndexA >> 1) & 1;
						const int32 AZ = (VD.IndexA >> 2) & 1;
						const int32 BX = VD.IndexB & 1;
						const int32 BY = (VD.IndexB >> 1) & 1;
						const int32 BZ = (VD.IndexB >> 2) & 1;
						const float FX = float(X) + FMath::Lerp(float(AX), float(BX), T);
						const float FY = float(Y) + FMath::Lerp(float(AY), float(BY), T);
						const float FZ = float(Z) + FMath::Lerp(float(AZ), float(BZ), T);

						const FVector Pos = FMath::Lerp(CornerPos(X, Y, Z, VD.IndexA), CornerPos(X, Y, Z, VD.IndexB), T);
						const int32 GI = Verts.Add(Pos);
						Normals.Add(GradientNormal(FX, FY, FZ));
						const FVector U = (Pos - UVOrigin) * InvCmPerUV;
						UV0.Add(FVector2D(U.X, U.Y));
						EdgeVerts.Add_CheckNew(Key, GI);
						LocalToGlobal[VI] = GI;
					}

					const Voxel::Transvoxel::FCellIndices& Indices =
						Voxel::Transvoxel::CellClassToCellIndices[Voxel::Transvoxel::GetCellClass(CellCode)];
					const int32 NumTris = Indices.NumTriangles();
					for (int32 TI = 0; TI < NumTris; ++TI)
					{
						Tris.Add(LocalToGlobal[Indices.GetIndex(TI * 3 + 0)]);
						Tris.Add(LocalToGlobal[Indices.GetIndex(TI * 3 + 1)]);
						Tris.Add(LocalToGlobal[Indices.GetIndex(TI * 3 + 2)]);
					}
				}
			}
		}

		Tangents.SetNum(Verts.Num());
		for (int32 I = 0; I < Verts.Num(); ++I)
		{
			const FVector& N = Normals[I];
			const FVector T = FMath::Abs(N.Z) < 0.9f
				? FVector::CrossProduct(N, FVector::UpVector).GetSafeNormal()
				: FVector::CrossProduct(N, FVector::RightVector).GetSafeNormal();
			Tangents[I] = FProcMeshTangent(T, false);
		}
	}
}
