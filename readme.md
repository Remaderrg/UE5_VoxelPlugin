# DG_VoxelPlugin

StaticMesh voxelization plugin: radius digging, small-cluster culling, `.voxel` save/load.

**Backend:** [VoxelCore](https://github.com/VoxelPlugin/VoxelCore) — density, bit array, falloff, Transvoxel, ParallelFor, ZipReader (legacy load), Project Settings.

- Make: surface → dilate → solid fill → density 0/1
- Dig: falloff carve coalesced to `RemeshHz` / stroke end (`DigDuration`); visual + collision remesh together
- Mesh: Transvoxel visual; hidden CollMesh with `CollisionStep` (coarser, always collision)
- Save: flush pending dig → **raw float Density** (no Oodle); Load reads v3 density (+ InitialSolidCount) + legacy v2/v1

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
| `dg.voxel.CarveThreshold` | density solid/air threshold for dig (default 0.5) |
| `dg.voxel.ParallelMake` | ParallelFor during voxelize |
| `dg.voxel.ParallelRemesh` | ParallelFor chunk generation |
| `dg.voxel.RemeshHz` | max remesh/carve rate while digging (default 5) |
| `dg.voxel.CollisionStep` | collision downsample vs visual (default 2; 1 = same) |
| `voxel.NoAsync` | (VoxelCore) force single thread |

## Blueprint

Category: `DG|Voxel`

Grid limit: **512**. Material — SourceMesh slot 0. Dig / Save / Reset resolve Voxel on **Self** when the Voxel pin is empty.

File: `{ProjectSavedDir}/Voxels/{Slot}.voxel`  
- Editor/PIE: `{ProjectDir}/Saved/Voxels/`
- Packaged: `%LOCALAPPDATA%\WinterGame\Saved\Voxels\` (wipe AppData = lose slots)
- Staged: `StagedBuilds/Windows/WinterGame/Saved/Voxels/` (or next to the `.exe`)

Log: `SaveToSlot OK/FAIL: <path>`.

### Setup

1. StaticMesh: **Allow CPU Access = true** → Save → **Package/Stage again** (required for Make in `.exe`). Engine BasicShapes (`Cube`) often ignore the flag — use a project copy of the mesh.
2. `Voxel` variable (optional — Save/Dig work without it via Self).
3. SlotName, e.g. `"Rock01"`.

### Startup

1. **Load or Make Voxel From Mesh** (Mesh, Slot [, Size, Cluster]) → SET `Voxel`.  
   Loads slot; if missing → Make → seed Save (recovers after wipe).

After signature changes: **re-wire the nodes** in BP (Compile). Remove old **Make Voxel** / **Load Voxel** nodes.

### Dig + Save

1. **Dig Voxel** (WorldLocation, Radius, Strength) — Voxel pin optional. Return = solid count in sphere (`Radius + VoxelSize`, Occupancy); falloff applies only to carve mesh. Strength scales density removal (default 1).
2. **Save Voxel** (SlotName) — Voxel pin optional; empty SlotName uses `ActiveSlot` from last Load/Save.

### Query

- **Search All Voxels** (WorldLocation, Radius) — solid count in sphere across **all** `UDG_VoxelComponent` in the world.
- **Voxel Info** (Voxel, SlotName) → **All** / **Remaining**.

### Reset

- **Reset Voxel** (Slot, Size, Cluster) — clean grid + delete slot file. Do not Destroy.

## `.voxel` format

Magic `VGXL`, version **3**. New saves: Flags=`0`, header includes `InitialSolidCount`, raw `float Density[N]`.  
Legacy v2: same density payload, no `InitialSolidCount` (All falls back to current solid).  
Legacy v1: packed occupancy bits; Flags `& 0x1`: zip/Oodle entry `bits` (read-only).

## Build

```powershell
pwsh ./RebuildPlugin.ps1
```

Requires sibling `VoxelCore`; builds via the parent project's `.uproject` + UBT.  
After Save changes: rebuild the **game** (not just the plugin) and Stage/Package again.
