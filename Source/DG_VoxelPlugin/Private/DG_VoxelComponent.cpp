// Copyright Demar Games. All Rights Reserved.

#include "DG_VoxelComponent.h"
#include "DG_VoxelCVars.h"
#include "DG_VoxelGrid.h"
#include "DG_VoxelTransvoxel.h"
#include "DG_VoxelSave.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "StaticMeshResources.h"
#include "VoxelZipReader.h"
#include "VoxelFalloff.h"

namespace DGVoxel
{
	template <typename T>
	static void AppendPod(TArray<uint8>& Out, T Value)
	{
		const uint8* Bytes = reinterpret_cast<const uint8*>(&Value);
		Out.Append(Bytes, sizeof(T));
	}

	template <typename T>
	static bool ReadPod(const TArray<uint8>& In, int32& Offset, T& OutValue)
	{
		if (Offset + (int32)sizeof(T) > In.Num())
		{
			return false;
		}
		FMemory::Memcpy(&OutValue, In.GetData() + Offset, sizeof(T));
		Offset += sizeof(T);
		return true;
	}

	static FBox ScaleBoxCorners(const FBox& Box, const FVector& Scale)
	{
		FBox Out(ForceInit);
		const FVector C[2] = { Box.Min, Box.Max };
		for (int32 IX = 0; IX < 2; ++IX)
		{
			for (int32 IY = 0; IY < 2; ++IY)
			{
				for (int32 IZ = 0; IZ < 2; ++IZ)
				{
					Out += FVector(C[IX].X, C[IY].Y, C[IZ].Z) * Scale;
				}
			}
		}
		return Out;
	}

	static EVoxelFalloffType ClampFalloffType(int32 Type)
	{
		return static_cast<EVoxelFalloffType>(FMath::Clamp(Type, 0, 4));
	}

	template <typename TFunc>
	static void ForIndices(int32 Count, bool bParallel, TFunc&& Func)
	{
		if (bParallel && Count > 1)
		{
			Voxel::ParallelFor(Count, Forward<TFunc>(Func));
		}
		else
		{
			for (int32 I = 0; I < Count; ++I)
			{
				Func(I);
			}
		}
	}
}

UDG_VoxelComponent::UDG_VoxelComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	Grid = new FDG_VoxelGrid();
	DigDirty = new FVoxelOptionalIntBox();
}

UDG_VoxelComponent::~UDG_VoxelComponent()
{
	delete DigDirty;
	DigDirty = nullptr;
	delete Grid;
	Grid = nullptr;
}

FString UDG_VoxelComponent::SlotFilePath(const FString& SlotName)
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Voxels"), SlotName + TEXT(".voxel")));
}

int32 UDG_VoxelComponent::NumChunks(int32 Dim) const
{
	return FMath::Max(1, (Dim + ChunkSize - 1) / ChunkSize);
}

int32 UDG_VoxelComponent::ChunkSectionIndex(int32 Cx, int32 Cy, int32 Cz) const
{
	return Cx + NumChunks(Grid->DimX) * (Cy + NumChunks(Grid->DimY) * Cz);
}

void UDG_VoxelComponent::RefreshGridToWorld()
{
	if (!SourceMesh)
	{
		GridToWorld = FTransform::Identity;
		return;
	}
	GridToWorld = SourceMesh->GetComponentTransform();
	GridToWorld.SetScale3D(FVector::OneVector);
}

bool UDG_VoxelComponent::EnsureProcMesh()
{
	AActor* Owner = GetOwner();
	USceneComponent* Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Owner || !Root || !SourceMesh)
	{
		return false;
	}

	RefreshGridToWorld();

	if (!ProcMesh)
	{
		ProcMesh = NewObject<UProceduralMeshComponent>(Owner, TEXT("DG_VoxelMesh"));
		ProcMesh->SetupAttachment(Root);
		ProcMesh->SetAbsolute(true, true, true);
		ProcMesh->bUseAsyncCooking = true;
		ProcMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		ProcMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Owner->AddInstanceComponent(ProcMesh);
		ProcMesh->RegisterComponent();
	}

	ProcMesh->SetWorldTransform(GridToWorld);
	return true;
}

void UDG_VoxelComponent::HideSourceMesh()
{
	if (!SourceMesh)
	{
		return;
	}
	SourceMesh->SetVisibility(false, true);
	SourceMesh->SetHiddenInGame(true, true);
	SourceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UDG_VoxelComponent::CacheVoxelMaterial()
{
	VoxelMaterial = nullptr;
	if (SourceMesh)
	{
		VoxelMaterial = SourceMesh->GetMaterial(0);
		if (!VoxelMaterial && SourceMesh->GetStaticMesh())
		{
			VoxelMaterial = SourceMesh->GetStaticMesh()->GetMaterial(0);
		}
	}
	if (!VoxelMaterial)
	{
		VOXEL_MESSAGE(Warning, "CacheVoxelMaterial: no material on source mesh");
	}
}

void UDG_VoxelComponent::ApplySectionMaterial(int32 Section) const
{
	if (ProcMesh && VoxelMaterial)
	{
		ProcMesh->SetMaterial(Section, VoxelMaterial);
	}
}

bool UDG_VoxelComponent::BuildFromMesh(UStaticMeshComponent* Mesh, float InVoxelSize, int32 InMinCluster)
{
	VOXEL_FUNCTION_COUNTER();

	if (!Mesh || !Mesh->GetStaticMesh() || !GetOwner())
	{
		VOXEL_MESSAGE(Warning, "BuildFromMesh: invalid mesh/owner");
		return false;
	}

	ClearDigState();
	DigDuration = FMath::Max(0.01f, GDG_VoxelDefaultDigDuration);
	SourceMesh = Mesh;
	VoxelSize = FMath::Max(1.f, InVoxelSize);
	MinClusterVoxels = FMath::Max(1, InMinCluster);
	CacheVoxelMaterial();

	if (!EnsureProcMesh())
	{
		VOXEL_MESSAGE(Warning, "BuildFromMesh: EnsureProcMesh failed");
		return false;
	}

	if (!VoxelizeStaticMesh(Mesh->GetStaticMesh()))
	{
		VOXEL_MESSAGE(Warning, "BuildFromMesh: VoxelizeStaticMesh failed");
		return false;
	}

	{
		FVoxelOptionalIntBox CullDirty;
		Grid->RemoveSmallClusters(MinClusterVoxels, CullDirty);
	}
	RebuildMesh();
	HideSourceMesh();
	return true;
}

bool UDG_VoxelComponent::VoxelizeStaticMesh(UStaticMesh* StaticMesh)
{
	VOXEL_FUNCTION_COUNTER();

	const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
	if (!RenderData || RenderData->LODResources.Num() == 0)
	{
		VOXEL_MESSAGE(Warning, "Voxelize: no LOD resources");
		return false;
	}

	const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
	const FPositionVertexBuffer& Positions = LOD.VertexBuffers.PositionVertexBuffer;
	const FRawStaticIndexBuffer& Indices = LOD.IndexBuffer;

	TArray<uint32> IndexCopy;
	Indices.GetCopy(IndexCopy);
	if (IndexCopy.Num() < 3 || Positions.GetAllocatedSize() == 0)
	{
		VOXEL_MESSAGE(Error,
			"Voxelize: '{0}' has no CPU mesh data (Allow CPU Access + re-save + recook). CachedIdx={1} StorageBytes={2} bAllowCPU={3}",
			StaticMesh->GetName(),
			Indices.GetNumIndices(),
			Indices.GetIndexDataSize(),
			(int32)StaticMesh->bAllowCPUAccess);
		return false;
	}

	const FVector Scale = SourceMesh ? SourceMesh->GetComponentScale() : FVector::OneVector;
	const FBox Bounds = DGVoxel::ScaleBoxCorners(StaticMesh->GetBoundingBox(), Scale);
	const FVector InOrigin = Bounds.Min;
	const FVector Ext = Bounds.GetSize();
	const int32 DimX = FMath::Max(1, FMath::CeilToInt(Ext.X / VoxelSize));
	const int32 DimY = FMath::Max(1, FMath::CeilToInt(Ext.Y / VoxelSize));
	const int32 DimZ = FMath::Max(1, FMath::CeilToInt(Ext.Z / VoxelSize));

	if (DimX > MaxDim || DimY > MaxDim || DimZ > MaxDim)
	{
		VOXEL_MESSAGE(Error, "Voxelize FAIL: grid {0}x{1}x{2} > MaxDim {3} (VS={4}). Increase VoxelSize.",
			DimX, DimY, DimZ, MaxDim, VoxelSize);
		return false;
	}

	Grid->Reset(DimX, DimY, DimZ, VoxelSize, InOrigin);

	const float MarkDistSq = FMath::Square(VoxelSize);
	const int32 TriCount = IndexCopy.Num() / 3;
	const float VS = VoxelSize;

	auto MarkTriangle = [&](int32 T)
	{
		const FVector A(FVector(Positions.VertexPosition(IndexCopy[T * 3 + 0])) * Scale);
		const FVector B(FVector(Positions.VertexPosition(IndexCopy[T * 3 + 1])) * Scale);
		const FVector C(FVector(Positions.VertexPosition(IndexCopy[T * 3 + 2])) * Scale);

		FBox TriBox(ForceInit);
		TriBox += A;
		TriBox += B;
		TriBox += C;
		TriBox = TriBox.ExpandBy(VS * 0.5f);

		const int32 X0 = FMath::Clamp(FMath::FloorToInt((TriBox.Min.X - InOrigin.X) / VS), 0, DimX - 1);
		const int32 Y0 = FMath::Clamp(FMath::FloorToInt((TriBox.Min.Y - InOrigin.Y) / VS), 0, DimY - 1);
		const int32 Z0 = FMath::Clamp(FMath::FloorToInt((TriBox.Min.Z - InOrigin.Z) / VS), 0, DimZ - 1);
		const int32 X1 = FMath::Clamp(FMath::FloorToInt((TriBox.Max.X - InOrigin.X) / VS), 0, DimX - 1);
		const int32 Y1 = FMath::Clamp(FMath::FloorToInt((TriBox.Max.Y - InOrigin.Y) / VS), 0, DimY - 1);
		const int32 Z1 = FMath::Clamp(FMath::FloorToInt((TriBox.Max.Z - InOrigin.Z) / VS), 0, DimZ - 1);

		for (int32 Z = Z0; Z <= Z1; ++Z)
		{
			for (int32 Y = Y0; Y <= Y1; ++Y)
			{
				for (int32 X = X0; X <= X1; ++X)
				{
					const FVector P = Grid->VoxelCenterLocal(X, Y, Z);
					if (FVoxelUtilities::PointTriangleDistanceSquared(P, A, B, C) <= MarkDistSq)
					{
						Grid->AtomicSetSolid(X, Y, Z, true);
					}
				}
			}
		}
	};

	DGVoxel::ForIndices(TriCount, GDG_VoxelParallelMake, [&](int32 T)
	{
		MarkTriangle(T);
	});

	Grid->DilateSolidOnce();
	Grid->FillInteriorFromSurface();
	RefreshGridToWorld();
	return true;
}

void UDG_VoxelComponent::ClearDigState()
{
	PendingDigs.Reset();
	DigRemovedSeeds.Reset();
	if (DigDirty)
	{
		DigDirty->Reset();
	}
	RemeshAccumulator = 0.f;
	SetComponentTickEnabled(false);
}

void UDG_VoxelComponent::MarkDigDirty(int32 X, int32 Y, int32 Z)
{
	*DigDirty += FIntVector(X, Y, Z);
}

void UDG_VoxelComponent::FlushDirtyMesh()
{
	VOXEL_FUNCTION_COUNTER();

	if (!DigDirty || !DigDirty->IsValid())
	{
		return;
	}

	const FVoxelIntBox& Box = DigDirty->GetBox();
	RebuildDirtyRegion(Box.Min.X, Box.Min.Y, Box.Min.Z, Box.Max.X - 1, Box.Max.Y - 1, Box.Max.Z - 1, true);
	DigDirty->Reset();
	RemeshAccumulator = 0.f;
}

void UDG_VoxelComponent::FinishPendingDigs()
{
	if (DigRemovedSeeds.Num() > 0)
	{
		for (const FIntVector& S : DigRemovedSeeds)
		{
			MarkDigDirty(S.X, S.Y, S.Z);
		}

		FVoxelOptionalIntBox CullDirty;
		Grid->RemoveSmallClusters(MinClusterVoxels, CullDirty);
		if (CullDirty.IsValid())
		{
			*DigDirty += CullDirty.GetBox();
		}
	}

	DigRemovedSeeds.Reset();
	if (DigDirty && DigDirty->IsValid())
	{
		FlushDirtyMesh();
	}
	SetComponentTickEnabled(false);
}

void UDG_VoxelComponent::FlushPendingDigsImmediate()
{
	if (PendingDigs.Num() == 0)
	{
		return;
	}

	for (FDigStroke& S : PendingDigs)
	{
		const FVoxelFalloff Falloff(DGVoxel::ClampFalloffType(S.FalloffType), S.FalloffAmount);
		Grid->CarveFalloffShell(S.CenterLocal, S.PrevR, S.TargetR, Falloff, DigRemovedSeeds, *DigDirty, S.Strength);
		S.PrevR = S.TargetR;
	}
	PendingDigs.Reset();
	FinishPendingDigs();
}

void UDG_VoxelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (PendingDigs.Num() == 0)
	{
		FinishPendingDigs();
		return;
	}

	const float Dur = FMath::Max(0.01f, DigDuration);
	for (int32 I = PendingDigs.Num() - 1; I >= 0; --I)
	{
		FDigStroke& S = PendingDigs[I];
		S.Age += DeltaTime;
		const float T = FMath::Clamp(S.Age / Dur, 0.f, 1.f);
		const float CurrR = S.TargetR * FMath::SmoothStep(0.f, 1.f, T);
		const FVoxelFalloff Falloff(DGVoxel::ClampFalloffType(S.FalloffType), S.FalloffAmount);
		Grid->CarveFalloffShell(S.CenterLocal, S.PrevR, CurrR, Falloff, DigRemovedSeeds, *DigDirty, S.Strength);
		S.PrevR = CurrR;
		if (T >= 1.f)
		{
			PendingDigs.RemoveAtSwap(I);
		}
	}

	if (PendingDigs.Num() == 0)
	{
		FinishPendingDigs();
		return;
	}

	if (!DigDirty || !DigDirty->IsValid())
	{
		return;
	}

	const float Hz = FMath::Max(1.f, GDG_VoxelRemeshHz);
	RemeshAccumulator += DeltaTime;
	if (RemeshAccumulator >= 1.f / Hz)
	{
		FlushDirtyMesh();
	}
}

void UDG_VoxelComponent::BuildChunkMesh(int32 Cx, int32 Cy, int32 Cz, FChunkMeshBuild& Out) const
{
	Out.Cx = Cx;
	Out.Cy = Cy;
	Out.Cz = Cz;
	Out.Verts.Reset();
	Out.Tris.Reset();
	Out.Normals.Reset();
	Out.UV0.Reset();
	Out.Tangents.Reset();

	if (Grid->NumBits() <= 0)
	{
		return;
	}

	const int32 DimX = Grid->DimX;
	const int32 DimY = Grid->DimY;
	const int32 DimZ = Grid->DimZ;
	const float VS = Grid->VoxelSize;

	const int32 CellX0 = (Cx == 0) ? -1 : Cx * ChunkSize;
	const int32 CellY0 = (Cy == 0) ? -1 : Cy * ChunkSize;
	const int32 CellZ0 = (Cz == 0) ? -1 : Cz * ChunkSize;
	const int32 CellX1 = FMath::Min((Cx + 1) * ChunkSize, DimX);
	const int32 CellY1 = FMath::Min((Cy + 1) * ChunkSize, DimY);
	const int32 CellZ1 = FMath::Min((Cz + 1) * ChunkSize, DimZ);

	const int32 Estimate = ChunkSize * ChunkSize * ChunkSize * 3;
	Out.Verts.Reserve(Estimate);
	Out.Tris.Reserve(Estimate);
	Out.Normals.Reserve(Estimate);
	Out.UV0.Reserve(Estimate);

	const FVector SampleOrigin = Grid->Origin + FVector(VS * 0.5f);
	DGVoxelTV::EmitChunk(
		CellX0, CellY0, CellZ0, CellX1, CellY1, CellZ1,
		VS, SampleOrigin,
		[this](int32 X, int32 Y, int32 Z) { return Grid->GetDensity(X, Y, Z); },
		Out.Verts, Out.Tris, Out.Normals, Out.UV0, Out.Tangents);
}

void UDG_VoxelComponent::UploadChunkMesh(const FChunkMeshBuild& Built, bool bCollision)
{
	if (!ProcMesh)
	{
		return;
	}

	const int32 Section = ChunkSectionIndex(Built.Cx, Built.Cy, Built.Cz);
	if (Built.Verts.Num() == 0)
	{
		ProcMesh->ClearMeshSection(Section);
		return;
	}

	static const TArray<FColor> EmptyColors;
	ProcMesh->CreateMeshSection(
		Section,
		Built.Verts,
		Built.Tris,
		Built.Normals,
		Built.UV0,
		EmptyColors,
		Built.Tangents,
		bCollision);
	ApplySectionMaterial(Section);
}

void UDG_VoxelComponent::RebuildChunks(const TArray<FIntVector>& ChunkCoords, bool bCollision)
{
	VOXEL_FUNCTION_COUNTER();

	if (ChunkCoords.Num() == 0 || !ProcMesh)
	{
		return;
	}

	TArray<FChunkMeshBuild> Built;
	Built.SetNum(ChunkCoords.Num());

	DGVoxel::ForIndices(ChunkCoords.Num(), GDG_VoxelParallelRemesh, [&](int32 Index)
	{
		const FIntVector& C = ChunkCoords[Index];
		BuildChunkMesh(C.X, C.Y, C.Z, Built[Index]);
	});

	for (const FChunkMeshBuild& Chunk : Built)
	{
		UploadChunkMesh(Chunk, bCollision);
	}
}

void UDG_VoxelComponent::RebuildDirtyRegion(int32 X0, int32 Y0, int32 Z0, int32 X1, int32 Y1, int32 Z1, bool bCollision)
{
	if (!EnsureProcMesh() || Grid->NumBits() <= 0)
	{
		return;
	}

	X0 = FMath::Clamp(X0 - 1, 0, Grid->DimX - 1);
	Y0 = FMath::Clamp(Y0 - 1, 0, Grid->DimY - 1);
	Z0 = FMath::Clamp(Z0 - 1, 0, Grid->DimZ - 1);
	X1 = FMath::Clamp(X1 + 1, 0, Grid->DimX - 1);
	Y1 = FMath::Clamp(Y1 + 1, 0, Grid->DimY - 1);
	Z1 = FMath::Clamp(Z1 + 1, 0, Grid->DimZ - 1);

	const int32 CX0 = X0 / ChunkSize;
	const int32 CY0 = Y0 / ChunkSize;
	const int32 CZ0 = Z0 / ChunkSize;
	const int32 CX1 = X1 / ChunkSize;
	const int32 CY1 = Y1 / ChunkSize;
	const int32 CZ1 = Z1 / ChunkSize;

	TArray<FIntVector> Chunks;
	Chunks.Reserve((CX1 - CX0 + 1) * (CY1 - CY0 + 1) * (CZ1 - CZ0 + 1));
	for (int32 Cz = CZ0; Cz <= CZ1; ++Cz)
	{
		for (int32 Cy = CY0; Cy <= CY1; ++Cy)
		{
			for (int32 Cx = CX0; Cx <= CX1; ++Cx)
			{
				Chunks.Emplace(Cx, Cy, Cz);
			}
		}
	}
	RebuildChunks(Chunks, bCollision);
}

void UDG_VoxelComponent::RebuildMesh()
{
	if (!EnsureProcMesh() || Grid->NumBits() <= 0)
	{
		return;
	}

	ProcMesh->ClearAllMeshSections();

	const int32 NX = NumChunks(Grid->DimX);
	const int32 NY = NumChunks(Grid->DimY);
	const int32 NZ = NumChunks(Grid->DimZ);
	TArray<FIntVector> Chunks;
	Chunks.Reserve(NX * NY * NZ);
	for (int32 Cz = 0; Cz < NZ; ++Cz)
	{
		for (int32 Cy = 0; Cy < NY; ++Cy)
		{
			for (int32 Cx = 0; Cx < NX; ++Cx)
			{
				Chunks.Emplace(Cx, Cy, Cz);
			}
		}
	}
	RebuildChunks(Chunks, true);
}

int32 UDG_VoxelComponent::DigAtWorld(const FVector& WorldLocation, float Radius, float Strength)
{
	if (Grid->NumBits() <= 0)
	{
		VOXEL_MESSAGE(Warning, "DigAtWorld: empty grid — Make/Load first");
		return 0;
	}
	if (!EnsureProcMesh() || Radius <= 0.f || Strength <= 0.f)
	{
		return 0;
	}

	RefreshGridToWorld();
	ProcMesh->SetWorldTransform(GridToWorld);

	const FVoxelFalloff Falloff(
		DGVoxel::ClampFalloffType(GDG_VoxelDigFalloffType),
		FMath::Clamp(GDG_VoxelDigFalloffAmount, 0.f, 1.f));
	const float CarveThreshold = FMath::Clamp(GDG_VoxelCarveThreshold, 0.f, 1.f);
	const float DigR = Radius + VoxelSize;
	const FVector Local = GridToWorld.InverseTransformPosition(WorldLocation);
	const int32 Estimate = Grid->CountSolidFalloff(Local, DigR, Falloff, CarveThreshold);
	if (Estimate <= 0)
	{
		return 0;
	}

	FDigStroke Stroke;
	Stroke.CenterLocal = Local;
	Stroke.TargetR = DigR;
	Stroke.FalloffType = static_cast<uint8>(Falloff.Type);
	Stroke.FalloffAmount = Falloff.Amount;
	Stroke.Strength = Strength;
	PendingDigs.Add(Stroke);
	SetComponentTickEnabled(true);
	return Estimate;
}

bool UDG_VoxelComponent::SaveToSlot(const FString& SlotName)
{
	VOXEL_FUNCTION_COUNTER();

	const FString EffectiveSlot = !SlotName.IsEmpty() ? SlotName : ActiveSlot;
	if (EffectiveSlot.IsEmpty())
	{
		VOXEL_MESSAGE(Warning, "SaveToSlot: empty SlotName");
		return false;
	}
	if (Grid->NumBits() <= 0)
	{
		VOXEL_MESSAGE(Warning, "SaveToSlot: empty grid — Make/Load first");
		return false;
	}

	FlushPendingDigsImmediate();

	TArray<uint8> Packed;
	Grid->PackDensity(Packed);
	const int32 BitCount = Grid->NumBits();

	TArray<uint8> Bytes;
	Bytes.Reserve(48 + Packed.Num());
	DGVoxel::AppendPod(Bytes, DG_VoxelMagic);
	DGVoxel::AppendPod(Bytes, DG_VoxelFormatVersion);
	const uint16 Flags = 0; // raw density floats
	DGVoxel::AppendPod(Bytes, Flags);
	DGVoxel::AppendPod(Bytes, Grid->DimX);
	DGVoxel::AppendPod(Bytes, Grid->DimY);
	DGVoxel::AppendPod(Bytes, Grid->DimZ);
	DGVoxel::AppendPod(Bytes, Grid->VoxelSize);
	DGVoxel::AppendPod(Bytes, (float)Grid->Origin.X);
	DGVoxel::AppendPod(Bytes, (float)Grid->Origin.Y);
	DGVoxel::AppendPod(Bytes, (float)Grid->Origin.Z);
	DGVoxel::AppendPod(Bytes, MinClusterVoxels);
	DGVoxel::AppendPod(Bytes, BitCount);
	Bytes.Append(Packed);

	const FString Path = SlotFilePath(EffectiveSlot);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveArrayToFile(Bytes, *Path))
	{
		VOXEL_MESSAGE(Error, "SaveToSlot FAIL: {0}", Path);
		return false;
	}
	ActiveSlot = EffectiveSlot;
	VOXEL_MESSAGE(Info, "SaveToSlot OK: {0} ({1} bytes)", Path, Bytes.Num());
	return true;
}

bool UDG_VoxelComponent::LoadFromSlot(const FString& SlotName)
{
	VOXEL_FUNCTION_COUNTER();

	if (SlotName.IsEmpty() || !SourceMesh)
	{
		VOXEL_MESSAGE(Warning, "LoadFromSlot: need SourceMesh + slot");
		return false;
	}

	ClearDigState();
	DigDuration = FMath::Max(0.01f, GDG_VoxelDefaultDigDuration);

	TArray<uint8> Bytes;
	const FString Path = SlotFilePath(SlotName);
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		VOXEL_MESSAGE(Warning, "LoadFromSlot: no file {0}", Path);
		return false;
	}

	int32 Off = 0;
	uint32 Magic = 0;
	uint16 Version = 0;
	uint16 Flags = 0;
	int32 InDimX = 0, InDimY = 0, InDimZ = 0;
	float InVoxelSize = 0.f;
	float Ox = 0.f, Oy = 0.f, Oz = 0.f;
	int32 InMinCluster = 0;
	int32 BitCount = 0;

	if (!DGVoxel::ReadPod(Bytes, Off, Magic) || Magic != DG_VoxelMagic
		|| !DGVoxel::ReadPod(Bytes, Off, Version)
		|| (Version != DG_VoxelFormatVersion && Version != DG_VoxelFormatVersion_Bits)
		|| !DGVoxel::ReadPod(Bytes, Off, Flags)
		|| !DGVoxel::ReadPod(Bytes, Off, InDimX)
		|| !DGVoxel::ReadPod(Bytes, Off, InDimY)
		|| !DGVoxel::ReadPod(Bytes, Off, InDimZ)
		|| !DGVoxel::ReadPod(Bytes, Off, InVoxelSize)
		|| !DGVoxel::ReadPod(Bytes, Off, Ox)
		|| !DGVoxel::ReadPod(Bytes, Off, Oy)
		|| !DGVoxel::ReadPod(Bytes, Off, Oz)
		|| !DGVoxel::ReadPod(Bytes, Off, InMinCluster)
		|| !DGVoxel::ReadPod(Bytes, Off, BitCount))
	{
		VOXEL_MESSAGE(Warning, "LoadFromSlot: bad header");
		return false;
	}

	if (InDimX <= 0 || InDimY <= 0 || InDimZ <= 0
		|| InDimX > MaxDim || InDimY > MaxDim || InDimZ > MaxDim
		|| BitCount != InDimX * InDimY * InDimZ
		|| InVoxelSize < 1.f)
	{
		VOXEL_MESSAGE(Warning, "LoadFromSlot: invalid dims");
		return false;
	}

	const bool bDensityV2 = Version == DG_VoxelFormatVersion;
	const int32 PackedBytes = bDensityV2
		? BitCount * (int32)sizeof(float)
		: (BitCount + 7) / 8;
	TArray<uint8> Packed;

	if (!bDensityV2 && (Flags & DG_VoxelFlag_ZipBits))
	{
		if (Off >= Bytes.Num())
		{
			VOXEL_MESSAGE(Warning, "LoadFromSlot: missing zip payload");
			return false;
		}

		const TConstVoxelArrayView64<uint8> ZipView(Bytes.GetData() + Off, Bytes.Num() - Off);
		const TSharedPtr<FVoxelZipReader> Reader = FVoxelZipReader::Create(ZipView);
		if (!Reader)
		{
			VOXEL_MESSAGE(Warning, "LoadFromSlot: zip reader failed");
			return false;
		}

		TVoxelArray64<uint8> Unpacked;
		if (!Reader->TryLoad(TEXT("bits"), Unpacked) || Unpacked.Num() < PackedBytes)
		{
			VOXEL_MESSAGE(Warning, "LoadFromSlot: zip bits missing/truncated");
			return false;
		}
		Packed.Append(Unpacked.GetData(), PackedBytes);
	}
	else
	{
		if (Off + PackedBytes > Bytes.Num())
		{
			VOXEL_MESSAGE(Warning, "LoadFromSlot: truncated payload");
			return false;
		}
		Packed.Append(Bytes.GetData() + Off, PackedBytes);
	}

	VoxelSize = InVoxelSize;
	MinClusterVoxels = FMath::Max(1, InMinCluster);
	Grid->Reset(InDimX, InDimY, InDimZ, InVoxelSize, FVector(Ox, Oy, Oz));
	const bool bOk = bDensityV2
		? Grid->UnpackDensity(Packed.GetData(), Packed.Num(), BitCount)
		: Grid->UnpackBits(Packed.GetData(), Packed.Num(), BitCount);
	if (!bOk)
	{
		VOXEL_MESSAGE(Warning, "LoadFromSlot: unpack failed");
		return false;
	}

	if (!EnsureProcMesh())
	{
		return false;
	}

	CacheVoxelMaterial();
	RebuildMesh();
	HideSourceMesh();
	ActiveSlot = SlotName;
	return true;
}
