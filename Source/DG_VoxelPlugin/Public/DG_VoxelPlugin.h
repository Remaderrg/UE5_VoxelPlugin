// Copyright Demar Games. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FDG_VoxelPluginModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
