// Copyright Demar Games. All Rights Reserved.

#include "DG_VoxelBPLibrary.h"
#include "DG_VoxelCVars.h"
#include "DG_VoxelComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDGVoxelBP, Log, All);

namespace
{
	static AActor* ResolveActor(const UObject* WorldContextObject)
	{
		if (!WorldContextObject)
		{
			return nullptr;
		}
		if (const AActor* AsActor = Cast<AActor>(WorldContextObject))
		{
			return const_cast<AActor*>(AsActor);
		}
		if (const UActorComponent* Comp = Cast<UActorComponent>(WorldContextObject))
		{
			return Comp->GetOwner();
		}
		if (const UObject* Outer = WorldContextObject->GetTypedOuter<AActor>())
		{
			return const_cast<AActor*>(Cast<AActor>(Outer));
		}
		return nullptr;
	}

	static UDG_VoxelComponent* ResolveVoxel(UDG_VoxelComponent* Voxel, const UObject* WorldContextObject)
	{
		if (Voxel)
		{
			return Voxel;
		}
		if (AActor* Actor = ResolveActor(WorldContextObject))
		{
			return Actor->FindComponentByClass<UDG_VoxelComponent>();
		}
		return nullptr;
	}

	static UDG_VoxelComponent* RequireVoxel(
		UDG_VoxelComponent* Voxel,
		const UObject* WorldContextObject,
		const TCHAR* FailMsg)
	{
		if (UDG_VoxelComponent* Resolved = ResolveVoxel(Voxel, WorldContextObject))
		{
			return Resolved;
		}
		UE_LOG(LogDGVoxelBP, Warning, TEXT("%s"), FailMsg);
		return nullptr;
	}

	static UDG_VoxelComponent* GetOrCreateVoxelOnOwner(AActor* Owner)
	{
		if (UDG_VoxelComponent* Existing = Owner->FindComponentByClass<UDG_VoxelComponent>())
		{
			return Existing;
		}
		UDG_VoxelComponent* Voxel = NewObject<UDG_VoxelComponent>(Owner, NAME_None, RF_Transactional);
		Owner->AddInstanceComponent(Voxel);
		Voxel->RegisterComponent();
		return Voxel;
	}

	static void ResolveMakeDefaults(float VoxelSize, int32 MinCluster, float& OutSize, int32& OutCluster)
	{
		OutSize = FMath::IsNearlyEqual(VoxelSize, 10.f) ? GDG_VoxelDefaultVoxelSize : VoxelSize;
		OutCluster = (MinCluster == 8) ? GDG_VoxelDefaultMinCluster : MinCluster;
	}
}

UDG_VoxelComponent* UDG_VoxelBPLibrary::MakeVoxel(
	UStaticMeshComponent* Mesh,
	float VoxelSize,
	int32 MinClusterVoxels)
{
	if (!Mesh || !Mesh->GetOwner() || !Mesh->GetStaticMesh())
	{
		UE_LOG(LogDGVoxelBP, Warning, TEXT("MakeVoxel: need Mesh + Owner + StaticMesh"));
		return nullptr;
	}

	float Size = 10.f;
	int32 Cluster = 8;
	ResolveMakeDefaults(VoxelSize, MinClusterVoxels, Size, Cluster);

	UDG_VoxelComponent* Voxel = GetOrCreateVoxelOnOwner(Mesh->GetOwner());
	if (!Voxel->BuildFromMesh(Mesh, Size, Cluster))
	{
		UE_LOG(LogDGVoxelBP, Warning, TEXT("MakeVoxel: BuildFromMesh failed"));
		return nullptr;
	}
	return Voxel;
}

int32 UDG_VoxelBPLibrary::DigVoxel(
	const UObject* WorldContextObject,
	FVector WorldLocation,
	float Radius,
	float Strength,
	UDG_VoxelComponent* Voxel)
{
	UDG_VoxelComponent* Resolved = RequireVoxel(Voxel, WorldContextObject, TEXT("DigVoxel: no voxel — Make/Load first"));
	return Resolved ? Resolved->DigAtWorld(WorldLocation, Radius, Strength) : 0;
}

bool UDG_VoxelBPLibrary::SaveVoxel(
	const UObject* WorldContextObject,
	const FString& SlotName,
	UDG_VoxelComponent* Voxel)
{
	UDG_VoxelComponent* Resolved = RequireVoxel(Voxel, WorldContextObject, TEXT("SaveVoxel: no voxel"));
	return Resolved && Resolved->SaveToSlot(SlotName);
}

bool UDG_VoxelBPLibrary::LoadVoxel(
	const UObject* WorldContextObject,
	const FString& SlotName,
	UDG_VoxelComponent* Voxel)
{
	UDG_VoxelComponent* Resolved = RequireVoxel(Voxel, WorldContextObject, TEXT("LoadVoxel: no voxel"));
	return Resolved && Resolved->LoadFromSlot(SlotName);
}

UDG_VoxelComponent* UDG_VoxelBPLibrary::LoadVoxelFromMesh(
	UStaticMeshComponent* Mesh,
	const FString& SlotName)
{
	if (!Mesh || !Mesh->GetOwner() || SlotName.IsEmpty())
	{
		UE_LOG(LogDGVoxelBP, Warning, TEXT("LoadVoxelFromMesh: need Mesh + Owner + SlotName"));
		return nullptr;
	}

	UDG_VoxelComponent* Voxel = GetOrCreateVoxelOnOwner(Mesh->GetOwner());
	Voxel->SourceMesh = Mesh;

	if (!Voxel->LoadFromSlot(SlotName))
	{
		return nullptr;
	}
	return Voxel;
}

bool UDG_VoxelBPLibrary::ResetVoxel(
	const UObject* WorldContextObject,
	const FString& SlotName,
	float VoxelSize,
	int32 MinClusterVoxels,
	UDG_VoxelComponent* Voxel)
{
	UDG_VoxelComponent* Resolved = ResolveVoxel(Voxel, WorldContextObject);
	if (!Resolved || !Resolved->SourceMesh)
	{
		UE_LOG(LogDGVoxelBP, Warning, TEXT("ResetVoxel: need Voxel + SourceMesh"));
		return false;
	}

	float Size = 10.f;
	int32 Cluster = 8;
	ResolveMakeDefaults(VoxelSize, MinClusterVoxels, Size, Cluster);

	if (!SlotName.IsEmpty())
	{
		IFileManager::Get().Delete(*UDG_VoxelComponent::SlotFilePath(SlotName));
	}

	return Resolved->BuildFromMesh(Resolved->SourceMesh, Size, Cluster);
}

int32 UDG_VoxelBPLibrary::SearchAllVoxels(
	const UObject* WorldContextObject,
	FVector WorldLocation,
	float Radius)
{
	if (!WorldContextObject || Radius <= 0.f)
	{
		return 0;
	}
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return 0;
	}

	int32 Total = 0;
	for (FActorIterator It(World); It; ++It)
	{
		TInlineComponentArray<UDG_VoxelComponent*> Voxels;
		It->GetComponents(Voxels);
		for (UDG_VoxelComponent* Voxel : Voxels)
		{
			if (Voxel)
			{
				Total += Voxel->QuerySolidInRadiusWorld(WorldLocation, Radius);
			}
		}
	}
	return Total;
}

void UDG_VoxelBPLibrary::VoxelInfo(
	const UObject* WorldContextObject,
	const FString& SlotName,
	UDG_VoxelComponent* Voxel,
	int32& All,
	int32& Remaining)
{
	All = 0;
	Remaining = 0;
	// SlotName kept for BP wiring (empty → ActiveSlot context); counts are live-only.
	(void)SlotName;
	if (UDG_VoxelComponent* Resolved = RequireVoxel(Voxel, WorldContextObject, TEXT("VoxelInfo: no voxel — Make/Load first")))
	{
		Resolved->GetVoxelInfo(All, Remaining);
	}
}
