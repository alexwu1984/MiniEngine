#include "Render/GBufferVisualization.h"
#include "Render/SceneTextures.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "core/system.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICachedStates.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHIRenderPass.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "Render/RDGUtils.h"

namespace Engine
{
	using namespace RenderCore;

	namespace
	{
		struct CBGBufferVisualize
		{
			int32_t Mode = 0;
			float CameraNearZ = 0.1f;
			float CameraFarZ = 1000.f;
			float Pad = 0.f;
		};
		using CBGBufferVisualizeWrap = TUniformBufferBinding<CBGBufferVisualize, 0u>;

		GraphicsPipelineStateInitializer MakeFullscreenPSO(std::shared_ptr<RHIVertexShader> VS, std::shared_ptr<RHIPixelShader> PS)
		{
			GraphicsPipelineStateInitializer Init;
			Init.VertexShader = std::move(VS);
			Init.PixelShader = std::move(PS);
			Init.BlendState = RHICachedStates::BlendDisable;
			Init.DepthStencilState = RHICachedStates::DepthStateDisable;
			Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;
			return Init;
		}
	}

	struct FGBufferVisualizationPassPrivate
	{
		explicit FGBufferVisualizationPassPrivate(DynamicRHI* InRHI)
			: RHI(InRHI)
		{
		}

		DynamicRHI* RHI = nullptr;
		std::shared_ptr<RHIVertexShader> VertexShader;
		std::shared_ptr<RHIPixelShader> PixelShader;
	};

	FGBufferVisualizationPass::FGBufferVisualizationPass(DynamicRHI* InRHI)
		: d_ptr(new FGBufferVisualizationPassPrivate(InRHI))
	{
	}

	FGBufferVisualizationPass::~FGBufferVisualizationPass()
	{
		delete d_ptr;
		d_ptr = nullptr;
	}

	void FGBufferVisualizationPass::InitResource()
	{
		C_P(FGBufferVisualizationPass);
		if (!d->RHI)
			return;
		const std::wstring Path = core::process_directory().wstring() + L"/ShaderLibDX/GBufferVisualize.hlsl";
		d->VertexShader = d->RHI->RHICreateVertexShader(Path, "VS_ScreenQuad", RHIVertexDeclare{}, {});
		d->PixelShader = d->RHI->RHICreatePixelShader(Path, "PS_GBufferVisualize", {});
	}

	void FGBufferVisualizationPass::Execute(RHICommandContext& RHIContext, const std::shared_ptr<FSceneTextures>& SceneTextures,
										  const FSceneViewData& ViewData, EGBufferVisualizeMode Mode) const
	{
		C_P(const FGBufferVisualizationPass);
		if (!d->RHI || !d->VertexShader || !d->PixelShader || !SceneTextures)
			return;
		if (Mode == EGBufferVisualizeMode::None)
			return;

		const std::shared_ptr<RHITexture2D> SceneColor = SceneTextures->GetSceneColor();
		const std::shared_ptr<RHITexture2D> PreLighting = SceneTextures->GetSceneColorPreLighting();
		const std::shared_ptr<RHITexture2D> Normal = SceneTextures->GetNormalBuffer();
		const std::shared_ptr<RHITexture2D> Emissive = SceneTextures->GetEmissiveBuffer();
		const std::shared_ptr<RHITexture2D> MR = SceneTextures->GetMetallicRoughnessBuffer();
		const std::shared_ptr<RHITexture2D> Depth = SceneTextures->GetDepth();
		const std::shared_ptr<RHITexture2D> MatAux = SceneTextures->GetMaterialAuxBuffer();
		if (!SceneColor || !Normal || !Emissive || !MR || !Depth)
			return;

		const std::shared_ptr<RHITexture2D> BaseColorSrc = PreLighting ? PreLighting : SceneColor;

		FRHIRenderPassDesc Om = FRHIRenderPassDesc::SingleColorNoDepth(SceneColor);
		Om.DebugName = "GBufferVisualization";
		FRDGUtils::AppendFullscreenDeclaredTextureBarriers(
			Om,
			{
				{"SceneColorPreLighting", BaseColorSrc},
				{"Normal", Normal},
				{"Emissive", Emissive},
				{"MetallicRoughness", MR},
				{"Depth", Depth},
				{"MaterialAux", MatAux},
				{"LitSceneColor", SceneColor},
			},
			SceneColor);
		FRHIRenderPassScope Scope(RHIContext, std::move(Om));

		CBGBufferVisualizeWrap Uniform(d->RHI);
		Uniform.Data.Mode = static_cast<int32_t>(Mode);
		Uniform.Data.CameraNearZ = ViewData.CameraNearZ;
		Uniform.Data.CameraFarZ = ViewData.CameraFarZ;

		RHIContext.RHISetGraphicsPipelineState(MakeFullscreenPSO(d->VertexShader, d->PixelShader));
		RHI_UpdateAndBindUniformBufferVSPS(RHIContext, Uniform);
		RHIContext.RHISetShaderSampler(SF_Pixel, 0, RHICachedStates::ClampLinerSampler);

		RHIContext.RHISetShaderTexture(SF_Pixel, 0, BaseColorSrc);
		RHIContext.RHISetShaderTexture(SF_Pixel, 1, Normal);
		RHIContext.RHISetShaderTexture(SF_Pixel, 2, Emissive);
		RHIContext.RHISetShaderTexture(SF_Pixel, 3, MR);
		RHIContext.RHISetShaderTexture(SF_Pixel, 4, Depth);
		RHIContext.RHISetShaderTexture(SF_Pixel, 5, MatAux ? MatAux : MR);
		RHIContext.RHISetShaderTexture(SF_Pixel, 6, SceneColor);

		RHIContext.Draw(3);
	}
}
