#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "ImageVRSGenerator.h"

class FImageVRSModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
