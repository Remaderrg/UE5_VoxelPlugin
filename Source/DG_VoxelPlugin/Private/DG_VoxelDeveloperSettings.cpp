// Copyright Demar Games. All Rights Reserved.

#include "DG_VoxelDeveloperSettings.h"
#include "DG_VoxelCVars.h"
#include "HAL/IConsoleManager.h"

UDG_VoxelDeveloperSettings::UDG_VoxelDeveloperSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("DG Voxel");
}

#if WITH_EDITOR
void UDG_VoxelDeveloperSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	GDG_VoxelDigFalloffType = static_cast<int32>(DigFalloffType);
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("dg.voxel.DigFalloffType")))
	{
		CVar->Set(GDG_VoxelDigFalloffType, ECVF_SetByProjectSetting);
	}
}
#endif

void UDG_VoxelDeveloperSettings::PostInitProperties()
{
	Super::PostInitProperties();
	DigFalloffType = static_cast<EVoxelFalloffType>(FMath::Clamp(GDG_VoxelDigFalloffType, 0, 4));
}
