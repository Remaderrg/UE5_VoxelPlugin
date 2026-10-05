# DG_VoxelPlugin

StaticMesh voxelization plugin: radius digging, small-cluster culling, `.voxel` save/load.

**Backend:** [VoxelCore](https://github.com/VoxelPlugin/VoxelCore) — density, bit array, falloff, Transvoxel, ParallelFor, ZipReader (legacy load), Project Settings.

- Make: surface → dilate → solid fill → density 0/1
- Dig: falloff carve on Tick (`DigDuration`); remesh ≤ `RemeshHz`
- Mesh: Transvoxel
- Save: flush pending dig → **raw packed bits** (no Oodle); Load reads raw and legacy zip

## Dependencies

```
WinterGame/Plugins/
  DG_VoxelPlugin/
  VoxelCore/          ← git clone https://github.com/VoxelPlugin/VoxelCore.git
```

## Project Settings / CVars

**Edit → Project Settings → Plugins → DG Voxel** (or console):

| CVar | Purpose |
|------|---------|
| `dg.voxel.DefaultVoxelSize` | Make default (when BP passes 10) |
| `dg.voxel.DefaultMinCluster` | Make default (when BP passes 8) |
| `dg.voxel.DefaultDigDuration` | DigDuration after Make/Load |
| `dg.voxel.DigFalloffType` | 0 None … 2 Smooth … 4 Tip |
| `dg.voxel.DigFalloffAmount` | rim falloff softness (radius fraction) |
| `dg.voxel.CarveThreshold` | falloff threshold for Dig early-out count |
| `dg.voxel.ParallelMake` | ParallelFor during voxelize |
| `dg.voxel.ParallelRemesh` | ParallelFor chunk generation |
| `dg.voxel.RemeshHz` | max remesh rate while digging (default 20; always with collision) |
| `voxel.NoAsync` | (VoxelCore) force single thread |

## Blueprint

Category: `DG|Voxel`

Grid limit: **512**. Material — SourceMesh slot 0. Dig / Save / Load / Reset resolve Voxel on **Self** when the Voxel pin is empty.

File: `{ProjectSavedDir}/Voxels/{Slot}.voxel`  
Staged: `StagedBuilds/Windows/WinterGame/Saved/Voxels/` (or next to the `.exe`). Log: `SaveToSlot OK/FAIL: <path>`.

### Setup

1. StaticMesh: **Allow CPU Access = true** → Save → Package.
2. `Voxel` variable (optional — Save/Dig work without it via Self).
3. SlotName, e.g. `"Rock01"`.

### Startup

1. **Load Voxel From Mesh** (Mesh, Slot) → SET `Voxel`.
2. Is Valid? No → **Make Voxel** → SET `Voxel`.

### Dig + Save

1. **Dig Voxel** (Location, Radius) — Voxel pin optional.
2. **Save Voxel** (SlotName) — Voxel pin optional.  
   After Save/Load/Reset signature changes: **re-wire the nodes** in BP (Compile).

### Load / Reset

- **Load Voxel** (Slot) — hot reload into a live component.
- **Reset Voxel** (Slot, Size, Cluster) — clean grid + delete slot file. Do not Destroy.

## `.voxel` format

Magic `VGXL`, version 1. New saves: Flags=`0`, raw packed bits.  
Legacy Flags `& 0x1`: zip/Oodle entry `bits` (read-only).

## Build

```powershell
pwsh ./RebuildPlugin.ps1
```

Requires sibling `VoxelCore`; builds via `WinterGame.uproject` + UBT.  
After Save changes: rebuild the **game** (not just the plugin) and Stage/Package again.
