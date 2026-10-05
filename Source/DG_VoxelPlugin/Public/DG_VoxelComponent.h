// Copyright Demar Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "DG_VoxelComponent.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
struct FDG_VoxelGrid;
struct FVoxelOptionalIntBox;

UCLASS(ClassGroup = (DG), meta = (BlueprintSpawnableComponent))
class DG_VOXELPLUGIN_API UDG_VoxelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxDim = 512;
	static constexpr int32 ChunkSize = 16;

	UDG_VoxelComponent();
	virtual ~UDG_VoxelComponent() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DG|Voxel", meta = (ClampMin = "1.0"))
	float VoxelSize = 10.f;

	/** Connected solid groups smaller than this are removed after dig. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DG|Voxel", meta = (ClampMin = "1"))
	int32 MinClusterVoxels = 8;

	/** Seconds for dig radius to grow 0 → target (smooth mesh deformation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DG|Voxel", meta = (ClampMin = "0.01"))
	float DigDuration = 0.2f;

	UPROPERTY(BlueprintReadOnly, Category = "DG|Voxel")
	TObjectPtr<UStaticMeshComponent> SourceMesh = nullptr;

	/** Cached from SourceMesh at Make — ProcMesh sections use this. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "DG|Voxel")
	TObjectPtr<UMaterialInterface> VoxelMaterial = nullptr;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool BuildFromMesh(UStaticMeshComponent* Mesh, float InVoxelSize, int32 InMinCluster);
	int32 DigAtWorld(const FVector& WorldLocation, float Radius, float Strength = 1.f);
	bool SaveToSlot(const FString& SlotName);
	bool LoadFromSlot(const FString& SlotName);

	static FString SlotFilePath(const FString& SlotName);

private:
	struct FDigStroke
	{
		FVector CenterLocal = FVector::ZeroVector;
		float TargetR = 0.f;
		float Age = 0.f;
		float PrevR = 0.f;
		uint8 FalloffType = 2;
		float FalloffAmount = 0.5f;
		float Strength = 1.f;
	};

	struct FChunkMeshBuild
	{
		int32 Cx = 0;
		int32 Cy = 0;
		int32 Cz = 0;
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UV0;
		TArray<FProcMeshTangent> Tangents;
	};

	/** Owned; incomplete type kept out of UHT (VoxelMinimal). Deleted in .cpp. */
	FDG_VoxelGrid* Grid = nullptr;
	FTransform GridToWorld = FTransform::Identity;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> ProcMesh = nullptr;

	TArray<FDigStroke> PendingDigs;
	TArray<FIntVector> DigRemovedSeeds;
	/** Heap-allocated — FVoxelOptionalIntBox lives in VoxelMinimal (not UHT-safe). */
	FVoxelOptionalIntBox* DigDirty = nullptr;
	float RemeshAccumulator = 0.f;

	int32 NumChunks(int32 Dim) const;
	int32 ChunkSectionIndex(int32 Cx, int32 Cy, int32 Cz) const;

	void RefreshGridToWorld();
	bool EnsureProcMesh();
	bool VoxelizeStaticMesh(UStaticMesh* StaticMesh);
	void BuildChunkMesh(int32 Cx, int32 Cy, int32 Cz, FChunkMeshBuild& Out) const;
	void UploadChunkMesh(const FChunkMeshBuild& Built, bool bCollision);
	void RebuildChunks(const TArray<FIntVector>& ChunkCoords, bool bCollision);
	void RebuildDirtyRegion(int32 X0, int32 Y0, int32 Z0, int32 X1, int32 Y1, int32 Z1, bool bCollision);
	void RebuildMesh();
	void HideSourceMesh();
	void CacheVoxelMaterial();
	void ApplySectionMaterial(int32 Section) const;

	void ClearDigState();
	void MarkDigDirty(int32 X, int32 Y, int32 Z);
	void FlushDirtyMesh();
	void FinishPendingDigs();
	/** Instant-complete pending dig strokes so Save packs carved occupancy. */
	void FlushPendingDigsImmediate();
};
