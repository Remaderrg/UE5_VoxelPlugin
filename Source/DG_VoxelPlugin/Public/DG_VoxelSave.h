// Copyright Demar Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Magic 'VGXL' little-endian. */
static constexpr uint32 DG_VoxelMagic = 0x4C584756u; // 'VGXL' LE
static constexpr uint16 DG_VoxelFormatVersion = 3;
static constexpr uint16 DG_VoxelFormatVersion_Density = 2; // raw float density, no InitialSolidCount
static constexpr uint16 DG_VoxelFormatVersion_Bits = 1; // legacy packed occupancy

/** Flags bit0: legacy zip/Oodle bits payload (read-only, v1 only). */
static constexpr uint16 DG_VoxelFlag_ZipBits = 0x1;

/**
 * Binary .voxel (little-endian): {ProjectSavedDir}/Voxels/{Slot}.voxel
 * Header: Magic u32, Version u16, Flags u16, DimX/Y/Z i32, VoxelSize f32,
 * Origin xyz f32, MinCluster i32, BitCount i32 (= DimX*DimY*DimZ),
 * InitialSolidCount i32 (v3+ only).
 * Payload v2/v3: raw float Density[N] (Flags=0).
 * Payload v1: packed occupancy bits (Flags=0) or zip entry "bits" (Flags&1).
 */
