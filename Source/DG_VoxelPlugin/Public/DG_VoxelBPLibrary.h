// Copyright Demar Games. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "DG_VoxelBPLibrary.generated.h"

class UDG_VoxelComponent;
class UStaticMeshComponent;

UCLASS()
class DG_VOXELPLUGIN_API UDG_VoxelBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "DG|Voxel", meta = (
		DisplayName = "Make Voxel",
		Keywords = "DG Voxel voxelize mesh"))
	static UDG_VoxelComponent* MakeVoxel(
		UStaticMeshComponent* Mesh,
		float VoxelSize = 10.f,
		int32 MinClusterVoxels = 8);

	UFUNCTION(BlueprintCallable, Category = "DG|Voxel", meta = (
		DisplayName = "Dig Voxel",
		Keywords = "DG Voxel dig carve remove",
		WorldContext = "WorldContextObject",
		DefaultToSelf = "WorldContextObject",
		AdvancedDisplay = "Voxel"))
	static int32 DigVoxel(
		const UObject* WorldContextObject,
		FVector WorldLocation,
		float Radius = 50.f,
		float Strength = 1.f,
		UDG_VoxelComponent* Voxel = nullptr);

	/** Voxel optional — if empty, finds UDG_VoxelComponent on Self (same as Dig). */
	UFUNCTION(BlueprintCallable, Category = "DG|Voxel", meta = (
		DisplayName = "Save Voxel",
		Keywords = "DG Voxel save slot",
		WorldContext = "WorldContextObject",
		DefaultToSelf = "WorldContextObject",
		AdvancedDisplay = "Voxel"))
	static bool SaveVoxel(
		const UObject* WorldContextObject,
		const FString& SlotName,
		UDG_VoxelComponent* Voxel = nullptr);

	UFUNCTION(BlueprintCallable, Category = "DG|Voxel", meta = (
		DisplayName = "Load Voxel",
		Keywords = "DG Voxel load slot",
		WorldContext = "WorldContextObject",
		DefaultToSelf = "WorldContextObject",
		AdvancedDisplay = "Voxel"))
	static bool LoadVoxel(
		const UObject* WorldContextObject,
		const FString& SlotName,
		UDG_VoxelComponent* Voxel = nullptr);

	UFUNCTION(BlueprintCallable, Category = "DG|Voxel", meta = (
		DisplayName = "Load Voxel From Mesh",
		Keywords = "DG Voxel load slot mesh"))
	static UDG_VoxelComponent* LoadVoxelFromMesh(
		UStaticMeshComponent* Mesh,
		const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "DG|Voxel", meta = (
		DisplayName = "Reset Voxel",
		Keywords = "DG Voxel reset clear slot",
		WorldContext = "WorldContextObject",
		DefaultToSelf = "WorldContextObject",
		AdvancedDisplay = "Voxel"))
	static bool ResetVoxel(
		const UObject* WorldContextObject,
		const FString& SlotName,
		float VoxelSize = 10.f,
		int32 MinClusterVoxels = 8,
		UDG_VoxelComponent* Voxel = nullptr);

	/** Sum solid voxels in Radius across every UDG_VoxelComponent in the world. */
	UFUNCTION(BlueprintPure, Category = "DG|Voxel", meta = (
		DisplayName = "Search All Voxels",
		Keywords = "DG Voxel count radius search world",
		WorldContext = "WorldContextObject",
		DefaultToSelf = "WorldContextObject"))
	static int32 SearchAllVoxels(
		const UObject* WorldContextObject,
		FVector WorldLocation,
		float Radius = 50.f);

	/** All = initial solid count; Remaining = solid on the map now. */
	UFUNCTION(BlueprintPure, Category = "DG|Voxel", meta = (
		DisplayName = "Voxel Info",
		Keywords = "DG Voxel info initial remaining count",
		WorldContext = "WorldContextObject",
		DefaultToSelf = "WorldContextObject"))
	static void VoxelInfo(
		const UObject* WorldContextObject,
		const FString& SlotName,
		UDG_VoxelComponent* Voxel,
		UPARAM(DisplayName = "All") int32& All,
		UPARAM(DisplayName = "Remaining") int32& Remaining);
};
