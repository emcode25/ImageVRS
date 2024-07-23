#include "ImageVRSGenerator.h"
#include "StereoRenderTargetManager.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "RenderGraphUtils.h"
#include "RHI.h"
#include "RHIDefinitions.h"
#include "RenderTargetPool.h"
#include "SystemTextures.h"
#include "SceneTextures.h"
#include "SceneRendering.h"
#include "SceneView.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "StereoRendering.h"
#include "SceneTexturesConfig.h"
#include "RenderGraphUtils.h"

FImageVRSGenerator GImageVRSGenerator;

/* CVar values used to control generator behavior */

TAutoConsoleVariable<int32> CVarCustomVRSImage(
	TEXT("r.VRS.CustomImage"),
	0,
	TEXT("Enables using Variable Rate Shading based on the luminance from the previous frame's post process output \n"),
	ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarCustomPreview(
	TEXT("r.VRS.CustomImage.Preview"),
	1,
	TEXT("Whether to use a custom image in the preview overlay.")
	TEXT("0 - off, 1 - on (default)"),
	ECVF_RenderThreadSafe);


/* The shader itself, which uses the custom image */

class FConvertVRSTexture : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FConvertVRSTexture);
	SHADER_USE_PARAMETER_STRUCT(FConvertVRSTexture, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, OutputTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FDataDrivenShaderPlatformInfo::GetSupportsVariableRateShading(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Shading rates:
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_1x1"), VRSSR_1x1);
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_1x2"), VRSSR_1x2);
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_2x1"), VRSSR_2x1);
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_2x2"), VRSSR_2x2);
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_2x4"), VRSSR_2x4);
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_4x2"), VRSSR_4x2);
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_4x4"), VRSSR_4x4);

		OutEnvironment.SetDefine(TEXT("SHADING_RATE_TILE_WIDTH"), GRHIVariableRateShadingImageTileMinWidth);
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_TILE_HEIGHT"), GRHIVariableRateShadingImageTileMinHeight);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), FComputeShaderUtils::kGolden2DGroupSize);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), FComputeShaderUtils::kGolden2DGroupSize);
	}

	static void InitParameters(
		FParameters& Parameters,
		FRDGTexture* InputTexture,
		FRDGTextureUAV* OutputTexture)
	{
		Parameters.InputTexture = InputTexture;
		Parameters.OutputTexture = OutputTexture;
	}
};

IMPLEMENT_GLOBAL_SHADER(FConvertVRSTexture, "/Plugin/ImageVRS/ConvertVRSTexture.usf", "GenerateVRSTexture", SF_Compute);

FRDGTextureRef FImageVRSGenerator::GetImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, FVariableRateShadingImageManager::EVRSImageType ImageType, bool bGetSoftwareImage)
{
	return CachedImage;
}

FIntPoint FImageVRSGenerator::GetSRITileSize(bool bSoftwareVRS)
{
	return bSoftwareVRS ? FIntPoint(2, 2) : FIntPoint(GRHIVariableRateShadingImageTileMinWidth, GRHIVariableRateShadingImageTileMinHeight);
}

FRDGTextureDesc FImageVRSGenerator::GetSRIDesc(const FSceneViewFamily& ViewFamily, bool bSoftwareVRS)
{
	check(!ViewFamily.Views.IsEmpty());
	check(ViewFamily.Views[0]->bIsViewInfo);


	const FSceneView* view = ViewFamily.Views[0];
	FIntRect viewRect = view->CameraConstrainedViewRect;
	FIntPoint SRISize = FMath::DivideAndRoundUp(viewRect.Size(), GetSRITileSize(bSoftwareVRS));
	
	return FRDGTextureDesc::Create2D(
		SRISize,
		bSoftwareVRS ? PF_R8_UINT : GRHIVariableRateShadingImageFormat,
		FClearValueBinding::None,
		TexCreate_Foveation | TexCreate_UAV | TexCreate_ShaderResource | TexCreate_DisableDCC);
}

void FImageVRSGenerator::PrepareImages(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, const FMinimalSceneTextures& SceneTextures, bool bPrepareHardwareImages, bool bPrepareSoftwareImages)
{
	//UE_LOG(LogVRS, Warning, TEXT("STARTING"));

	if (!bPrepareHardwareImages)
	{
		// Software images unsupported for now
		return;
	}

	// Sanity check VRS tile size.
	check(GRHIVariableRateShadingImageTileMinWidth >= 8 && GRHIVariableRateShadingImageTileMinWidth <= 64 && GRHIVariableRateShadingImageTileMinHeight >= 8 && GRHIVariableRateShadingImageTileMinHeight <= 64);

	//Store texture as usable shader resource
	//Code from @Inconceivable on the Unreal forums
	//https://forums.unrealengine.com/t/rdg-shader-with-utexture-input/574861/2

	//Get the render info from the image
	FSceneRenderTargetItem texItem;
	texItem.TargetableTexture = userImage->GetResource()->GetTexture2DRHI();
	texItem.ShaderResourceTexture = userImage->GetResource()->GetTexture2DRHI();

	//Store information about the render target
	FPooledRenderTargetDesc renderTargetDesc =
		FPooledRenderTargetDesc::Create2DDesc(
			FIntPoint(userImage->GetSizeX(), userImage->GetSizeY()),
			userImage->GetPixelFormat(),
			FClearValueBinding::None,
			TexCreate_None,
			TexCreate_ShaderResource | TexCreate_UAV,
			false);

	//Create the input render target to read from and register it so shaders can use it
	TRefCountPtr<IPooledRenderTarget> pooledRenderTarget;
	GRenderTargetPool.CreateUntrackedElement(renderTargetDesc, pooledRenderTarget, texItem);
	FRDGTextureRef InputTexture = GraphBuilder.RegisterExternalTexture(pooledRenderTarget, TEXT("UserVRSTexture"));


	//Create an output render target that will contain the VRS data
	FRDGTextureDesc Desc = GetSRIDesc(ViewFamily, bPrepareSoftwareImages);
	FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(Desc, TEXT("OutputVRSTexture"));
	FRDGTextureUAVRef OutputTextureUAV = GraphBuilder.CreateUAV(OutputTexture);
	//AddClearUAVPass(GraphBuilder, OutputTextureUAV, static_cast<uint32>(0xA));


	//Send the textures to the shader
	FConvertVRSTexture::FParameters* PassParameters = GraphBuilder.AllocParameters<FConvertVRSTexture::FParameters>();
	FConvertVRSTexture::InitParameters(*PassParameters, InputTexture, OutputTextureUAV);


	//Dispatch the shader
	TShaderMapRef<FConvertVRSTexture> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(Desc.Extent, FComputeShaderUtils::kGolden2DGroupSize);
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("ComputeConvertVRSTexture"),
		PassParameters,
		ERDGPassFlags::Compute,
		[PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
		{
			FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
		});

	//Store the newly created image and set the generated flag
	CachedImage = OutputTexture;
	generateImage = false;
}

bool FImageVRSGenerator::IsEnabled() const
{
	return CVarCustomVRSImage.GetValueOnRenderThread() == 1;
}

bool FImageVRSGenerator::IsSupportedByView(const FSceneView& View) const
{
	return true;
}

FVariableRateShadingImageManager::EVRSSourceType FImageVRSGenerator::GetType() const
{
	return FVariableRateShadingImageManager::EVRSSourceType::CustomImage;
}

FRDGTextureRef FImageVRSGenerator::GetDebugImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, FVariableRateShadingImageManager::EVRSImageType ImageType, bool bGetSoftwareImage)
{
	return CachedImage;
}

void FImageVRSGenerator::SetTexture(UTexture2D* tex)
{
	userImage = tex;
	generateImage = true;
}