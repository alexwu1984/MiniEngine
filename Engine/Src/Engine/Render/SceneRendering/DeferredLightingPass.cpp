#include "Render/SceneRendering/DeferredLightingPass.h"
#include "Render/RDGUtils.h"
#include "Render/SceneTextures.h"
#include "Render/WorldSceneRender.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "Render/PreProcessor.h"
#include "Render/SkyLightEnvironment.h"
#include "Render/Shadow/ShadowRenderPass.h"
#include "Render/MaterialPreFrame.h"
#include "RHI/RHIRenderTarget.h"
#include "RHI/RHITextureCube.h"
#include "RHI/RHIDefinitions.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIViewPort.h"
#include "core/system.h"
#include <algorithm>

namespace Engine
{
	using namespace RenderCore;

	namespace
	{
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

		static void FillPerFrameFromView(CBPerFrameWrap& Out, const FSceneViewData& View, PreProcessor* Pre, FWorldSceneRender* WorldSceneRender)
		{
			Out.Data.myPerFrame.CameraPrevViewProj = View.PrevViewProjMatrix;
			Out.Data.myPerFrame.CameraCurrViewProj = View.CurrViewProjMatrix;
			Out.Data.myPerFrame.CameraCurrViewProjInverse = View.CurrViewProjInverseMatrix;
			math::Matrix4x4 Rotate = math::Matrix4x4::RotateX(math::Radians(View.EnvironmentRotatePitchDegrees));
			Rotate *= math::Matrix4x4::RotateY(math::Radians(View.EnvironmentRotateYawDegrees));
			Out.Data.myPerFrame.RotateIBL = Rotate;
			Out.Data.myPerFrame.CameraPos = View.CameraPos;
			Out.Data.myPerFrame.TemporalAAJitter = View.TemporalAAJitter;
			if (View.ViewRectSizeX > 0 && View.ViewRectSizeY > 0)
			{
				Out.Data.myPerFrame.InvScreenResolution.x = 1.f / static_cast<float>(View.ViewRectSizeX);
				Out.Data.myPerFrame.InvScreenResolution.y = 1.f / static_cast<float>(View.ViewRectSizeY);
			}
			Out.Data.myPerFrame.CameraNearZ = View.CameraNearZ;
			Out.Data.myPerFrame.CameraFarZ = View.CameraFarZ;
			Out.Data.myPerFrame.IBLFactor = View.SkyLightIBLScale;
			const int32_t n = (std::min)(static_cast<int32_t>(View.Lights.size()), static_cast<int32_t>(MAX_LIGHT_INSTANCES));
			Out.Data.myPerFrame.LightCount = n;
			Out.Data.myPerFrame.bUnlit = 0;
			if (n > 0)
			{
				for (int32_t i = 0; i < n; ++i)
					Out.Data.myPerFrame.Lights[i] = View.Lights[(size_t)i];
			}
			else
				Out.Data.myPerFrame.Lights[0] = Light{};
			if (WorldSceneRender && !View.Lights.empty())
			{
				if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
				{
					Light L{};
					if (ShadowPass->TryGetCachedMainLightForShading(L))
					{
						Out.Data.myPerFrame.Lights[0].LightView = L.LightView;
						Out.Data.myPerFrame.Lights[0].LightViewProj = L.LightViewProj;
						Out.Data.myPerFrame.Lights[0].ShadowMapIndex = L.ShadowMapIndex;
						Out.Data.myPerFrame.Lights[0].Position = L.Position;
					}
				}
			}
			// CB says shadow on but no map bound -> PS would sample undefined t8 (hang / corruption).
			if (WorldSceneRender && Out.Data.myPerFrame.Lights[0].ShadowMapIndex >= 0)
			{
				if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
				{
					const std::shared_ptr<RHIRenderTarget> ShadowRt = ShadowPass->GetShadowMap();
					if (!ShadowRt || !ShadowRt->GetTex())
						Out.Data.myPerFrame.Lights[0].ShadowMapIndex = -1;
				}
			}
			Out.Data.myPerFrame.Material.Metallic = 0.f;
			Out.Data.myPerFrame.Material.padding0 = 0;
			Out.Data.myPerFrame.Material.padding1 = 0;
			Out.Data.myPerFrame.Material.padding2 = 0;
			if (Pre)
			{
				if (auto SkyLightEnv = Pre->GetSkyLightEnvironment())
				{
					if (auto SpecCube = SkyLightEnv->GetSpecularReflectionCubemap())
						Out.Data.myPerFrame.IBLMIpCount = static_cast<float>(std::max<uint32_t>(SpecCube->GetNumMips(), 1u));
					else
						Out.Data.myPerFrame.IBLMIpCount = 1.f;
				}
				else
					Out.Data.myPerFrame.IBLMIpCount = 1.f;
			}
			else
				Out.Data.myPerFrame.IBLMIpCount = 1.f;
		}
	} // namespace

	DeferredLightingPass::DeferredLightingPass(DynamicRHI* InRHI)
		: RHI(InRHI)
	{
	}

	void DeferredLightingPass::InitResource()
	{
		const std::wstring Path = core::process_directory().wstring() + L"/ShaderLibDX/DeferredLighting.hlsl";
		VertexShader = RHI->RHICreateVertexShader(Path, "VS_ScreenQuad", {}, {});
		PixelShader = RHI->RHICreatePixelShader(Path, "PS_DeferredLighting", {});
		if (RHI && !FallbackIBLCube)
			FallbackIBLCube = RHI->RHICreateTextureCube(EPixelFormat::PF_FloatRGBA, 2, 2, 1, false);
		if (RHI && !FallbackBrdfLut)
		{
			float brdfRg[4] = { 0.f, 0.f, 0.f, 1.f };
			FallbackBrdfLut = RHI->RHICreateTexture2D(EPixelFormat::PF_G32R32F, static_cast<int32_t>(ETextureCreateFlags::TexCreate_ShaderResource), 1, 1, 1, brdfRg, 16);
		}
	}

	void DeferredLightingPass::CopySceneColorToPreLighting(RHICommandContext& RHIContext, const std::shared_ptr<SceneTextures>& TargetBuffer) const
	{
		if (!TargetBuffer)
			return;
		const std::shared_ptr<RHITexture2D> SceneColorPreLighting = TargetBuffer->GetSceneColorPreLighting();
		const std::shared_ptr<RHITexture2D> SceneColor = TargetBuffer->GetSceneColor();
		if (!SceneColorPreLighting || !SceneColor)
			return;

		RHICommandMark Mark(RHIContext, "DeferredLighting_CopySceneColor");
		// RHICopyResource(dst, src): preserve base-pass output in PreLighting; raster pass overwrites SceneColor.
		RHIContext.RHICopyResource(SceneColorPreLighting, SceneColor);
	}

	void DeferredLightingPass::ExecuteRaster(RHICommandContext& RHIContext, std::shared_ptr<RHIViewPort> ViewPort, const std::shared_ptr<SceneTextures>& TargetBuffer,
											 FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData) const
	{
		if (!RHI || !VertexShader || !PixelShader || !TargetBuffer || !ViewData || !ViewPort)
			return;
		const std::shared_ptr<RHITexture2D> SceneColorPreLighting = TargetBuffer->GetSceneColorPreLighting();
		const std::shared_ptr<RHITexture2D> SceneColor = TargetBuffer->GetSceneColor();
		if (!SceneColorPreLighting || !SceneColor)
			return;

		RHICommandMark Mark(RHIContext, "DeferredLighting");

		PreProcessor* Pre = nullptr;
		if (WorldSceneRender)
			Pre = WorldSceneRender->GetPreProcessor().get();

		CBPerFrameWrap PerFrameCB(RHI);
		FillPerFrameFromView(PerFrameCB, *ViewData, Pre, WorldSceneRender);

		FRDGUtils::RHICmdListSetRenderTargetSingleColorNoDepth(RHIContext, SceneColor);
		FRDGUtils::RHICmdListSetViewportFromTexture(RHIContext, SceneColor);
		RHIContext.RHISetGraphicsPipelineState(MakeFullscreenPSO(VertexShader, PixelShader));

		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, PerFrameCB);

		RHIContext.RHISetShaderSampler(SF_Pixel, 0, RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderSampler(SF_Pixel, 1, RHICachedStates::ShadowSampler);

		RHIContext.RHISetShaderTexture(SF_Pixel, 0, SceneColorPreLighting);
		RHIContext.RHISetShaderTexture(SF_Pixel, 1, TargetBuffer->GetNormalBuffer());
		RHIContext.RHISetShaderTexture(SF_Pixel, 2, TargetBuffer->GetEmissiveBuffer());
		RHIContext.RHISetShaderTexture(SF_Pixel, 3, TargetBuffer->GetMetallicRoughnessBuffer());
		RHIContext.RHISetShaderTexture(SF_Pixel, 4, TargetBuffer->GetDepth());

		std::shared_ptr<RHITextureCube> irrCube = FallbackIBLCube;
		std::shared_ptr<RHITextureCube> specCube = FallbackIBLCube;
		std::shared_ptr<RHITexture2D> brdfLut = FallbackBrdfLut;
		if (Pre)
		{
			if (const auto SkyLightEnv = Pre->GetSkyLightEnvironment())
			{
				if (const auto t = SkyLightEnv->GetDiffuseIrradianceCubemap())
					irrCube = t;
				if (const auto t = SkyLightEnv->GetSpecularReflectionCubemap())
					specCube = t;
				if (const auto t = SkyLightEnv->GetBRDFIntegrationLUT())
					brdfLut = t;
			}
		}
		RHIContext.RHISetShaderTexture(SF_Pixel, 5, irrCube);
		RHIContext.RHISetShaderTexture(SF_Pixel, 6, brdfLut);
		RHIContext.RHISetShaderTexture(SF_Pixel, 7, specCube);

		// Must match CB filled above: view lights keep ShadowMapIndex == -1 (e.g. DirectionalLightComponent); shadow pass patches CB via TryGetCachedMainLightForShading.
		const bool bDeferredShadow = WorldSceneRender && PerFrameCB.Data.myPerFrame.LightCount > 0
			&& PerFrameCB.Data.myPerFrame.Lights[0].ShadowMapIndex >= 0;
		// RHISetShaderTexture ignores nullptr; leaving t8 unstaged keeps whatever was bound last frame (validation / GPU faults).
		std::shared_ptr<RHITexture2D> shadowSrvTex = FallbackBrdfLut;
		if (bDeferredShadow)
		{
			if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
			{
				if (const std::shared_ptr<RHIRenderTarget> shadowRt = ShadowPass->GetShadowMap())
				{
					if (std::shared_ptr<RHITexture2D> st = shadowRt->GetTex())
						shadowSrvTex = std::move(st);
				}
			}
		}
		RHIContext.RHISetShaderTexture(SF_Pixel, 8, shadowSrvTex);

		std::shared_ptr<RHITexture2D> materialAuxSrv = FallbackBrdfLut;
		if (std::shared_ptr<RHITexture2D> ma = TargetBuffer->GetMaterialAuxBuffer())
			materialAuxSrv = std::move(ma);
		RHIContext.RHISetShaderTexture(SF_Pixel, 9, materialAuxSrv);

		RHIContext.Draw(3);
	}

	void DeferredLightingPass::Execute(RHICommandContext& RHIContext, std::shared_ptr<RHIViewPort> ViewPort, const std::shared_ptr<SceneTextures>& TargetBuffer,
									   FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData) const
	{
		CopySceneColorToPreLighting(RHIContext, TargetBuffer);
		ExecuteRaster(RHIContext, std::move(ViewPort), TargetBuffer, WorldSceneRender, ViewData);
	}
}
