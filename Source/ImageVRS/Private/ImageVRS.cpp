#include "ImageVRS.h"
#include "VariableRateShadingImageManager.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FImageVRSModule"

void FImageVRSModule::StartupModule()
{;
	FString ShaderDirectory = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ImageVRS/Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/ImageVRS"), ShaderDirectory);
	
	//Add generator to the global VRS image manager
	GVRSImageManager.RegisterExternalImageGenerator(&GImageVRSGenerator);
}

void FImageVRSModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FImageVRSModule, ImageVRS)