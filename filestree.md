# DG_VoxelPlugin — project layout

```
WinterGame/Plugins/
├── VoxelCore/                          # sibling: https://github.com/VoxelPlugin/VoxelCore
└── DG_VoxelPlugin/
    ├── DG_VoxelPlugin.uplugin          # ProceduralMeshComponent + VoxelCore
    ├── RebuildPlugin.ps1               # UBT via sibling .uproject
    ├── readme.md
    ├── filestree.md
    ├── Config/
    │   └── FilterPlugin.ini
    ├── Resources/
    │   └── Icon128.png
    ├── Content/
    │
    └── Source/DG_VoxelPlugin/
        ├── DG_VoxelPlugin.Build.cs     # VoxelCore, C++20
        ├── Public/
        │   ├── DG_VoxelPlugin.h
        │   ├── DG_VoxelComponent.h     # BP props; PIMPL grid; dig strokes
        │   ├── DG_VoxelBPLibrary.h     # Make / Dig / Save / Load / Reset (Self-resolve)
        │   ├── DG_VoxelSave.h          # VGXL density f32 v2 (+ legacy bits/zip)
        │   ├── DG_VoxelCVars.h         # dg.voxel.* externs
        │   ├── DG_VoxelDeveloperSettings.h  # Project Settings → UVoxelDeveloperSettings
        │   └── DG_VoxelTransvoxel.h    # Voxel::Transvoxel emitter
        └── Private/
            ├── DG_VoxelPlugin.cpp
            ├── DG_VoxelCVars.cpp       # VOXEL_CONSOLE_VARIABLE
            ├── DG_VoxelDeveloperSettings.cpp
            ├── DG_VoxelGrid.h          # Density + FVoxelBitArray + falloff carve
            ├── DG_VoxelGrid.cpp
            ├── DG_VoxelComponent.cpp   # throttled remesh, Transvoxel, file IO
            └── DG_VoxelBPLibrary.cpp
```
