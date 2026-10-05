// Copyright Demar Games. All Rights Reserved.

#pragma once

#include "VoxelDeveloperSettings.h"
#include "VoxelFalloff.h"
#include "DG_VoxelDeveloperSettings.generated.h"

/**
 * Project Settings → Plugins → DG Voxel.
 * Values sync to dg.voxel.* CVars via UVoxelDeveloperSettings.
 */
UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "DG Voxel"))
class DG_VOXELPLUGIN_API UDG_VoxelDeveloperSettings : public UVoxelDeveloperSettings
{
	GENERATED_BODY()

public:
	UDG_VoxelDeveloperSettings();

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(Config, EditAnywhere, Category = "Defaults", meta = (
		ClampMin = "1.0",
		ConsoleVariable = "dg.voxel.DefaultVoxelSize",
		DisplayName = "Default Voxel Size"))
	float DefaultVoxelSize = 10.f;

	UPROPERTY(Config, EditAnywhere, Category = "Defaults", meta = (
		ClampMin = "1",
		ConsoleVariable = "dg.voxel.DefaultMinCluster",
		DisplayName = "Default Min Cluster"))
	int32 DefaultMinCluster = 8;

	UPROPERTY(Config, EditAnywhere, Category = "Defaults", meta = (
		ClampMin = "0.01",
		ConsoleVariable = "dg.voxel.DefaultDigDuration",
		DisplayName = "Default Dig Duration"))
	float DefaultDigDuration = 0.2f;

	/** Synced to dg.voxel.DigFalloffType in PostEditChangeProperty (enum ↔ int CVar). */
	UPROPERTY(Config, EditAnywhere, Category = "Dig", meta = (DisplayName = "Dig Falloff Type"))
	EVoxelFalloffType DigFalloffType = EVoxelFalloffType::Smooth;

	UPROPERTY(Config, EditAnywhere, Category = "Dig", meta = (
		ClampMin = "0.0", ClampMax = "1.0",
		ConsoleVariable = "dg.voxel.DigFalloffAmount",
		DisplayName = "Dig Falloff Amount"))
	float DigFalloffAmount = 0.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Dig", meta = (
		ClampMin = "0.0", ClampMax = "1.0",
		ConsoleVariable = "dg.voxel.CarveThreshold",
		DisplayName = "Carve Threshold"))
	float CarveThreshold = 0.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (
		ConsoleVariable = "dg.voxel.ParallelMake",
		DisplayName = "Parallel Make"))
	bool bParallelMake = true;

	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (
		ConsoleVariable = "dg.voxel.ParallelRemesh",
		DisplayName = "Parallel Remesh"))
	bool bParallelRemesh = true;

	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (
		ClampMin = "1.0", ClampMax = "120.0",
		ConsoleVariable = "dg.voxel.RemeshHz",
		DisplayName = "Dig Remesh Hz"))
	float RemeshHz = 20.f;
};
