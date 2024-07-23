#pragma once

#include "Engine/Engine.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImageVRSManager.generated.h"

UCLASS()
class UImageVRSManager : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set VRS Texture", Keywords = "vrs image variable rate shading"), Category = "ImageVRS")
	static void SetVRSTexture(UTexture2D* texture);
};