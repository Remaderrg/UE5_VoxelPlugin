// Copyright Demar Games. All Rights Reserved.

#include "DG_VoxelCVars.h"
#include "VoxelMinimal.h"

VOXEL_CONSOLE_VARIABLE(
	DG_VOXELPLUGIN_API, float, GDG_VoxelDefaultVoxelSize, 10.f,
	"dg.voxel.DefaultVoxelSize",
	"Default voxel size (cm) used when Make Voxel passes the library default");

VOXEL_CONSOLE_VARIABLE(
	DG_VOXELPLUGIN_API, int32, GDG_VoxelDefaultMinCluster, 8,
	"dg.voxel.DefaultMinCluster",
	"Default min cluster size used when Make Voxel passes the library default");

VOXEL_CONSOLE_VARIABLE(
	DG_VOXELPLUGIN_API, float, GDG_VoxelDefaultDigDuration, 0.2f,
	"dg.voxel.DefaultDigDuration",
	"Default dig duration (seconds) applied on Make");

VOXEL_CONSOLE_VARIABLE(
	DG_VOXELPLUGIN_API, int32, GDG_VoxelDigFalloffType, 2,
	"dg.voxel.DigFalloffType",
	"EVoxelFalloffType: 0=None 1=Linear 2=Smooth 3=Spherical 4=Tip");

VOXEL_CONSOLE_VARIABLE(
	DG_VOXELPLUGIN_API, float, GDG_VoxelDigFalloffAmount, 0.5f,
	"dg.voxel.DigFalloffAmount",
	"Dig falloff amount (0..1); soft rim as fraction of dig radius");

VOXEL_CONSOLE_VARIABLE(
	DG_VOXELPLUGIN_API, float, GDG_VoxelCarveThreshold, 0.5f,
	"dg.voxel.CarveThreshold",
	"Falloff weight threshold to remove a solid voxel (0..1)");

VOXEL_CONSOLE_VARIABLE(
	DG_VOXELPLUGIN_API, bool, GDG_VoxelParallelMake, true,
	"dg.voxel.ParallelMake",
	"Use Voxel::ParallelFor during Make voxelize (respects voxel.NoAsync)");

VOXEL_CONSOLE_VARIABLE(
	DG_VOXELPLUGIN_API, bool, GDG_VoxelParallelRemesh, true,
	"dg.voxel.ParallelRemesh",
	"Use Voxel::ParallelFor for chunk mesh generation (respects voxel.NoAsync)");

VOXEL_CONSOLE_VARIABLE(
	DG_VOXELPLUGIN_API, float, GDG_VoxelRemeshHz, 20.f,
	"dg.voxel.RemeshHz",
	"Max visual remesh rate during dig (Hz); collision remesh always at stroke end");
