#include "ImageVRSManager.h"
#include "ImageVRS.h"

void UImageVRSManager::SetVRSTexture(UTexture2D* tex)
{
	GImageVRSGenerator.SetTexture(tex);
}