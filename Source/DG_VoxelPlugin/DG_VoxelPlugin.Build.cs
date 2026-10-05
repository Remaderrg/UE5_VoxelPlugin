// Copyright Demar Games. All Rights Reserved.

using UnrealBuildTool;

public class DG_VoxelPlugin : ModuleRules
{
	public DG_VoxelPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"ProceduralMeshComponent",
			"RenderCore",
			"RHI",
			"VoxelCore"
		});
	}
}
