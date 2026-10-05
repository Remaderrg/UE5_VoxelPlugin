// Copyright Demar Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Magic 'VGXL' little-endian. */
static constexpr uint32 DG_VoxelMagic = 0x4C584756u; // 'VGXL' LE
static constexpr uint16 DG_VoxelFormatVersion = 1;

/** Flags bit0: legacy zip/Oodle payload (read-only). New saves use raw packed bits (Flags=0). */
static constexpr uint16 DG_VoxelFlag_ZipBits = 0x1;

/**
 * Binary .voxel (little-endian): {ProjectSavedDir}/Voxels/{Slot}.voxel
 * Header: Magic u32, Version u16, Flags u16, DimX/Y/Z i32, VoxelSize f32,
 * Origin xyz f32, MinCluster i32, BitCount i32.
 * Payload: raw packed occupancy (Flags=0) or legacy zip entry "bits" (Flags&1).
 */
