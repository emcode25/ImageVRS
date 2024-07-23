#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RendererInterface.h"
#include "RenderGraphDefinitions.h"
#include "VariableRateShadingImageManager.h"
#include "Engine/Engine.h"

class FImageVRSGenerator : public IVariableRateShadingImageGenerator
{
public:
	virtual ~FImageVRSGenerator() override {};

	// Returns cached VRS image.
	virtual FRDGTextureRef GetImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, FVariableRateShadingImageManager::EVRSImageType ImageType, bool bGetSoftwareImage = false) override;

	// Generates image(s) and saves to generator cache. Should only be run once per view per frame, in Render().
	virtual void PrepareImages(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, const FMinimalSceneTextures& SceneTextures, bool bPrepareHardwareImages, bool bPrepareSoftwareImages) override;

	// Returns whether or not generator is enabled - can change at runtime
	virtual bool IsEnabled() const override;

	// Returns whether or not the given view supports this generator
	virtual bool IsSupportedByView(const FSceneView& View) const override;

	// Return bitmask of generator type
	virtual FVariableRateShadingImageManager::EVRSSourceType GetType() const override;

	// Get VRS image to be used w/ debug overlay
	virtual FRDGTextureRef GetDebugImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, FVariableRateShadingImageManager::EVRSImageType ImageType, bool bGetSoftwareImage = false) override;

	// Set the specified image to use for VRS
	IMAGEVRS_API void SetTexture(UTexture2D* tex);

private:
	FIntPoint GetSRITileSize(bool bSoftwareVRS);
	FRDGTextureDesc GetSRIDesc(const FSceneViewFamily& ViewFamily, bool bSoftwareVRS);

	bool generateImage = true;
	UTexture2D* userImage = nullptr;
	FRDGTextureRef CachedImage = nullptr;
};

IMAGEVRS_API extern FImageVRSGenerator GImageVRSGenerator;