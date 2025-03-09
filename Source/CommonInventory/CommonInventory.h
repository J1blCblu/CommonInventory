// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FCommonInventoryModule final : public IModuleInterface
{
public:
	
	//virtual void StartupModule() override;
	//virtual void ShutdownModule() override;

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}
};
