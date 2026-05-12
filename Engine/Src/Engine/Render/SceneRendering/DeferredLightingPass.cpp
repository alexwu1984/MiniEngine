#include "Render/SceneRendering/DeferredLightingPass.h"
#include "Render/RDGUtils.h"
#include "Render/SceneTextures.h"
#include "Render/WorldSceneRender.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "Render/SkyLightEnvironment.h"
#include "Render/Shadow/ShadowRenderPass.h"
#include "Render/MaterialPreFrame.h"
#include "RHI/RHIRenderTarget.h"
#include "RHI/RHITextureCube.h"
#include "RHI/RHIDefinitions.h"
#include "RHI/RHIStructuredBuffer.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIViewPort.h"
#include "core/commandline.h"
#include "core/logger.h"
#include "core/system.h"
#include "core/wall_timer.h"
#include <algorithm>

namespace Engine
{
	using namespace RenderCore;

	namespace
	{
		// Capacity ceiling for the forward path's StructuredBuffer<Light>. Picked to give clustered Forward+ headroom
		// (still well under typical 64KB CB / SRV sizing) without making cluster culling pass over an oversized list.
		// 256 * sizeof(Light) (== ~144 bytes on this engine) ≈ 36 KB GPU mem per slot, ×3 ring slots ≈ 108 KB.
		constexpr uint32_t kSceneLightBufferCapacity = 256u;

		/** SF_Pixel slot for `StructuredBuffer<Light> _SceneLights` in forward translucent + fur HLSL. */
		constexpr uint32_t kSceneLightsSrvSlot = 13u;
		/** SF_Pixel slots for clustered Forward+ outputs (RWStructuredBuffer in CS, StructuredBuffer in PS). */
		constexpr uint32_t kClusterLightOffsetCountSrvSlot = 14u;
		constexpr uint32_t kClusterLightIndexListSrvSlot = 15u;

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

		static void FillPerFrameFromView(CBPerFrameWrap& Out, CBPointShadowWrap* OutPointShadow, CBSpotShadowWrap* OutSpotShadow,
										 CBDirectionalShadowWrap* OutDirectionalShadow, const FSceneViewData& View, USkyLightComponent* SkyLightIBL,
										 FWorldSceneRender* WorldSceneRender)
		{
			Out.Data.myPerFrame.CameraPrevViewProj = View.PrevViewProjMatrix;
			Out.Data.myPerFrame.CameraCurrViewProj = View.CurrViewProjMatrix;
			Out.Data.myPerFrame.CameraCurrViewProjInverse = View.CurrViewProjInverseMatrix;
			Out.Data.myPerFrame.RotateIBL = math::Matrix4x4::ms_Materix3X3WIdentity;
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
			Out.Data.myPerFrame.IBLDirShadowCoupling = math::Vector4(View.IBLDiffuseDirShadowCoupling, View.IBLSpecularDirShadowCoupling,
																	 View.IBLDiffuseAoExponentForIBL, 0.f);
			Out.Data.myPerFrame.SplitHemisphereIBL = 0;
			Out.Data.myPerFrame.GroundIBLIntensity = 1.f;
			Out.Data.myPerFrame.HemiIBLBlendPower = 1.75f;
			const int32_t n = (std::min)(static_cast<int32_t>(View.Lights.size()), static_cast<int32_t>(MAX_LIGHT_INSTANCES));
			Out.Data.myPerFrame.LightCount = n;
			Out.Data.myPerFrame.bUnlit = 0;
			int32_t primaryDirIdx = -1;
			for (int32_t i = 0; i < n; ++i)
			{
				if (View.Lights[(size_t)i].Type == LightType_Directional)
				{
					primaryDirIdx = i;
					break;
				}
			}
			Out.Data.myPerFrame.PrimaryDirectionalLightIndex = primaryDirIdx;
			if (n > 0)
			{
				for (int32_t i = 0; i < n; ++i)
					Out.Data.myPerFrame.Lights[i] = View.Lights[(size_t)i];
			}
			else
				Out.Data.myPerFrame.Lights[0] = Light{};
			if (WorldSceneRender && primaryDirIdx >= 0)
			{
				if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
				{
					Light L{};
					if (ShadowPass->TryGetCachedMainLightForShading(L))
					{
						Light& Slot = Out.Data.myPerFrame.Lights[primaryDirIdx];
						Slot.LightView = L.LightView;
						Slot.LightViewProj = L.LightViewProj;
						Slot.ShadowMapIndex = L.ShadowMapIndex;
						Slot.Position = L.Position;
					}
				}
			}
			// CB says shadow on but no map bound -> PS would sample undefined t8 (hang / corruption).
			if (WorldSceneRender && primaryDirIdx >= 0 && Out.Data.myPerFrame.Lights[primaryDirIdx].ShadowMapIndex >= 0)
			{
				if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
				{
					const std::shared_ptr<RHIRenderTarget> ShadowRt = ShadowPass->GetShadowMap();
					if (!ShadowRt || !ShadowRt->GetTex())
						Out.Data.myPerFrame.Lights[primaryDirIdx].ShadowMapIndex = -1;
				}
			}
			// Single directional shadow map (t8): only PrimaryDirectionalLightIndex may reference it; extra directionals stay analytic-only.
			if (primaryDirIdx >= 0)
			{
				for (int32_t i = 0; i < n; ++i)
				{
					if (i != primaryDirIdx && Out.Data.myPerFrame.Lights[(size_t)i].Type == LightType_Directional)
						Out.Data.myPerFrame.Lights[(size_t)i].ShadowMapIndex = -1;
				}
			}
			if (OutPointShadow)
			{
				OutPointShadow->Data.Enabled = 0;
				OutPointShadow->Data.LightIndex = -1;
				if (WorldSceneRender)
				{
					if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
					{
						int li = -1;
						math::Matrix4x4 fvp[6];
						math::Vector4 pr{};
						if (ShadowPass->TryGetCachedPointShadowForDeferred(li, fvp, pr) && ShadowPass->GetPointShadowCube())
						{
							for (int fi = 0; fi < 6; ++fi)
								OutPointShadow->Data.FaceVP[fi] = fvp[fi];
							OutPointShadow->Data.LightPosRange = pr;
							OutPointShadow->Data.Enabled = 1;
							OutPointShadow->Data.LightIndex = li;
						}
					}
				}
				if (OutPointShadow->Data.Enabled == 0)
				{
					for (int32_t i = 0; i < n; ++i)
					{
						if (Out.Data.myPerFrame.Lights[(size_t)i].ShadowMapIndex == kPointLightCubeShadowMapIndex)
							Out.Data.myPerFrame.Lights[(size_t)i].ShadowMapIndex = -1;
					}
				}
				else
				{
					const int32_t cubeOwner = OutPointShadow->Data.LightIndex;
					for (int32_t i = 0; i < n; ++i)
					{
						if (Out.Data.myPerFrame.Lights[(size_t)i].Type != LightType_Point)
							continue;
						if (Out.Data.myPerFrame.Lights[(size_t)i].ShadowMapIndex == kPointLightCubeShadowMapIndex && i != cubeOwner)
							Out.Data.myPerFrame.Lights[(size_t)i].ShadowMapIndex = -1;
					}
				}
			}
			else
			{
				for (int32_t i = 0; i < n; ++i)
				{
					if (Out.Data.myPerFrame.Lights[i].ShadowMapIndex == kPointLightCubeShadowMapIndex)
						Out.Data.myPerFrame.Lights[i].ShadowMapIndex = -1;
				}
			}
			if (OutSpotShadow)
			{
				OutSpotShadow->Data.SpotShadowEnabled = 0;
				OutSpotShadow->Data.SpotShadowLightIndex = -1;
				if (WorldSceneRender)
				{
					if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
					{
						int li = -1;
						math::Matrix4x4 spotVp{};
						if (ShadowPass->TryGetCachedSpotShadowForDeferred(li, spotVp) && ShadowPass->GetSpotShadowMap())
						{
							const std::shared_ptr<RHIRenderTarget> spotRt = ShadowPass->GetSpotShadowMap();
							if (spotRt && spotRt->GetTex())
							{
								OutSpotShadow->Data.SpotLightViewProj = spotVp;
								OutSpotShadow->Data.SpotShadowEnabled = 1;
								OutSpotShadow->Data.SpotShadowLightIndex = li;
							}
						}
					}
				}
				if (OutSpotShadow->Data.SpotShadowEnabled == 0)
				{
					for (int32_t i = 0; i < n; ++i)
					{
						if (Out.Data.myPerFrame.Lights[(size_t)i].ShadowMapIndex == kSpotLightShadowMapIndex)
							Out.Data.myPerFrame.Lights[(size_t)i].ShadowMapIndex = -1;
					}
				}
				else
				{
					const int32_t spotOwner = OutSpotShadow->Data.SpotShadowLightIndex;
					for (int32_t i = 0; i < n; ++i)
					{
						if (Out.Data.myPerFrame.Lights[(size_t)i].Type != LightType_Spot)
							continue;
						if (Out.Data.myPerFrame.Lights[(size_t)i].ShadowMapIndex == kSpotLightShadowMapIndex && i != spotOwner)
							Out.Data.myPerFrame.Lights[(size_t)i].ShadowMapIndex = -1;
					}
				}
			}
			else
			{
				for (int32_t i = 0; i < n; ++i)
				{
					if (Out.Data.myPerFrame.Lights[i].ShadowMapIndex == kSpotLightShadowMapIndex)
						Out.Data.myPerFrame.Lights[i].ShadowMapIndex = -1;
				}
			}
			if (SkyLightIBL)
			{
				if (auto SpecCube = SkyLightIBL->GetSpecularReflectionCubemap())
					Out.Data.myPerFrame.IBLMIpCount = static_cast<float>(std::max<uint32_t>(SpecCube->GetNumMips(), 1u));
				else
					Out.Data.myPerFrame.IBLMIpCount = 1.f;
				if (SkyLightIBL->HasSplitHemisphereGroundIBL())
				{
					Out.Data.myPerFrame.SplitHemisphereIBL = 1;
					Out.Data.myPerFrame.GroundIBLIntensity = SkyLightIBL->GetGroundIBLIntensityForShader();
					Out.Data.myPerFrame.HemiIBLBlendPower = SkyLightIBL->GetHemiIBLBlendPowerForShader();
				}
			}
			else
				Out.Data.myPerFrame.IBLMIpCount = 1.f;

			if (OutDirectionalShadow)
			{
				OutDirectionalShadow->Data = CBDirectionalShadow{};
				if (WorldSceneRender)
				{
					if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
						OutDirectionalShadow->Data = ShadowPass->GetCachedDirectionalShadow();
				}
			}
		}
	} // namespace

	DeferredLightingPass::DeferredLightingPass(DynamicRHI* InRHI)
		: RHI(InRHI)
	{
	}

	void DeferredLightingPass::InitResource()
	{
		core::WallSplitTimer Wall;
		// VS/PS JIT in EnsureJitDeferredLightingShaders (first ExecuteRaster) so ReloadSceneJson / flush is not blocked by FXC.
		if (RHI && !FallbackIBLCube)
			FallbackIBLCube = RHI->RHICreateTextureCube(EPixelFormat::PF_FloatRGBA, 2, 2, 1, false);
		if (RHI && !FallbackBrdfLut)
		{
			float brdfRg[4] = { 0.f, 0.f, 0.f, 1.f };
			FallbackBrdfLut = RHI->RHICreateTexture2D(EPixelFormat::PF_G32R32F, static_cast<int32_t>(ETextureCreateFlags::TexCreate_ShaderResource), 1, 1, 1, brdfRg, 16);
		}
		const double MsFallbackTex = Wall.split_ms();
		if (RHI)
		{
			PerFrameUniform = std::make_unique<CBPerFrameWrap>(RHI);
			if (!PerFrameUniform->GetRHIBuffer())
				PerFrameUniform.reset();
			PointShadowUniform = std::make_unique<CBPointShadowWrap>(RHI);
			if (!PointShadowUniform->GetRHIBuffer())
				PointShadowUniform.reset();
			SpotShadowUniform = std::make_unique<CBSpotShadowWrap>(RHI);
			if (!SpotShadowUniform->GetRHIBuffer())
				SpotShadowUniform.reset();
			DirectionalShadowUniform = std::make_unique<CBDirectionalShadowWrap>(RHI);
			if (!DirectionalShadowUniform->GetRHIBuffer())
				DirectionalShadowUniform.reset();
		}
		const double MsUniforms = Wall.split_ms();
		const double MsBuffersWall = MsFallbackTex + MsUniforms;
		const double MsTotal = Wall.total_ms();
		core::inf() << core::perf::hdr(core::perf::kShaderJit, "DeferredLightingInit") << "wall_ms=" << MsBuffersWall << " fallback_tex_ms=" << MsFallbackTex
					<< " uniform_buffers_ms=" << MsUniforms << " total_ms=" << MsTotal
					<< " note=screen_quad_vs_ps_jit_on_first_ExecuteRaster_Perf|shader_jit|DeferredLightingJitShaders\n";
	}

	void DeferredLightingPass::EnsureJitDeferredLightingShaders() const
	{
		if (VertexShader && PixelShader)
			return;
		if (!RHI)
			return;
		core::WallSplitTimer Wall;
		const std::wstring Path = core::process_directory().wstring() + L"/ShaderLibDX/DeferredLighting.hlsl";
		VertexShader = RHI->RHICreateVertexShader(Path, "VS_ScreenQuad", {}, {});
		const double MsVs = Wall.split_ms();
		PixelShader = RHI->RHICreatePixelShader(Path, "PS_DeferredLighting", {});
		const double MsPs = Wall.split_ms();

		static constexpr double kPerfShaderJitLogMinWallMs = 10.0;
		const bool bVerboseJit = core::CommandLine::Get().GetSwitch("perfshaderjitverbose");
		const double MsJitWall = MsVs + MsPs;
		if (bVerboseJit || MsJitWall >= kPerfShaderJitLogMinWallMs)
		{
			core::inf() << core::perf::hdr(core::perf::kShaderJit, "DeferredLightingJitShaders") << "wall_ms=" << MsJitWall << " vs_ms=" << MsVs << " ps_ms=" << MsPs << "\n";
		}
	}

	void DeferredLightingPass::CopySceneColorToPreLighting(RHICommandContext& RHIContext, const std::shared_ptr<FSceneTextures>& SceneTextures) const
	{
		if (!SceneTextures)
			return;
		const std::shared_ptr<RHITexture2D> SceneColorPreLighting = SceneTextures->GetSceneColorPreLighting();
		const std::shared_ptr<RHITexture2D> SceneColor = SceneTextures->GetSceneColor();
		if (!SceneColorPreLighting || !SceneColor)
			return;

		RHICommandMark Mark(RHIContext, "DeferredLighting_CopySceneColor");
		// RHICopyResource(dst, src): preserve base-pass output in PreLighting; raster pass overwrites SceneColor.
		RHIContext.RHICopyResource(SceneColorPreLighting, SceneColor);
	}

	void DeferredLightingPass::ExecuteRaster(RHICommandContext& RHIContext, std::shared_ptr<RHIViewPort> ViewPort, const std::shared_ptr<FSceneTextures>& SceneTextures,
											 FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData) const
	{
		EnsureJitDeferredLightingShaders();
		if (!RHI || !VertexShader || !PixelShader || !SceneTextures || !ViewData || !ViewPort)
			return;
		if (!PerFrameUniform || !PerFrameUniform->GetRHIBuffer())
			return;
		const std::shared_ptr<RHITexture2D> SceneColorPreLighting = SceneTextures->GetSceneColorPreLighting();
		const std::shared_ptr<RHITexture2D> SceneColor = SceneTextures->GetSceneColor();
		if (!SceneColorPreLighting || !SceneColor)
			return;

		RHICommandMark Mark(RHIContext, "DeferredLighting");

		USkyLightComponent* SkyLightIBL = nullptr;
		if (WorldSceneRender)
			SkyLightIBL = WorldSceneRender->GetUSkyLightComponent().get();

		FillPerFrameFromView(*PerFrameUniform, PointShadowUniform ? PointShadowUniform.get() : nullptr, SpotShadowUniform ? SpotShadowUniform.get() : nullptr,
							 DirectionalShadowUniform ? DirectionalShadowUniform.get() : nullptr, *ViewData, SkyLightIBL, WorldSceneRender);

		FRDGUtils::RHICmdListSetRenderTargetSingleColorNoDepth(RHIContext, SceneColor);
		FRDGUtils::RHICmdListSetViewportFromTexture(RHIContext, SceneColor);
		RHIContext.RHISetGraphicsPipelineState(MakeFullscreenPSO(VertexShader, PixelShader));

		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, *PerFrameUniform);
		if (PointShadowUniform && PointShadowUniform->GetRHIBuffer())
			RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, *PointShadowUniform);
		if (SpotShadowUniform && SpotShadowUniform->GetRHIBuffer())
			RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, *SpotShadowUniform);
		if (DirectionalShadowUniform && DirectionalShadowUniform->GetRHIBuffer())
			RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, *DirectionalShadowUniform);

		RHIContext.RHISetShaderSampler(SF_Pixel, 0, RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderSampler(SF_Pixel, 1, RHICachedStates::ShadowSampler);
		RHIContext.RHISetShaderSampler(SF_Pixel, 2, RHICachedStates::ShadowCompareSampler);

		RHIContext.RHISetShaderTexture(SF_Pixel, 0, SceneColorPreLighting);
		RHIContext.RHISetShaderTexture(SF_Pixel, 1, SceneTextures->GetNormalBuffer());
		RHIContext.RHISetShaderTexture(SF_Pixel, 2, SceneTextures->GetEmissiveBuffer());
		RHIContext.RHISetShaderTexture(SF_Pixel, 3, SceneTextures->GetMetallicRoughnessBuffer());
		RHIContext.RHISetShaderTexture(SF_Pixel, 4, SceneTextures->GetDepth());

		std::shared_ptr<RHITextureCube> irrCube = FallbackIBLCube;
		std::shared_ptr<RHITextureCube> specCube = FallbackIBLCube;
		std::shared_ptr<RHITexture2D> brdfLut = FallbackBrdfLut;
		if (SkyLightIBL)
		{
			if (const auto t = SkyLightIBL->GetDiffuseIrradianceCubemap())
				irrCube = t;
			if (const auto t = SkyLightIBL->GetSpecularReflectionCubemap())
				specCube = t;
			if (const auto t = SkyLightIBL->GetBRDFIntegrationLUT())
				brdfLut = t;
		}
		RHIContext.RHISetShaderTexture(SF_Pixel, 5, irrCube);
		RHIContext.RHISetShaderTexture(SF_Pixel, 6, brdfLut);
		RHIContext.RHISetShaderTexture(SF_Pixel, 7, specCube);

		// Must match CB filled above: view lights keep ShadowMapIndex == -1 (e.g. DirectionalLightComponent); shadow pass patches CB via TryGetCachedMainLightForShading.
		const int32_t pdi = PerFrameUniform->Data.myPerFrame.PrimaryDirectionalLightIndex;
		const bool bDeferredShadow = WorldSceneRender && PerFrameUniform->Data.myPerFrame.LightCount > 0 && pdi >= 0
			&& PerFrameUniform->Data.myPerFrame.Lights[pdi].Type == LightType_Directional
			&& PerFrameUniform->Data.myPerFrame.Lights[pdi].ShadowMapIndex >= 0;
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
		if (std::shared_ptr<RHITexture2D> ma = SceneTextures->GetMaterialAuxBuffer())
			materialAuxSrv = std::move(ma);
		RHIContext.RHISetShaderTexture(SF_Pixel, 9, materialAuxSrv);

		std::shared_ptr<RHITextureCube> pointShadowSrv = FallbackIBLCube;
		if (PointShadowUniform && PointShadowUniform->GetRHIBuffer() && PointShadowUniform->Data.Enabled != 0 && WorldSceneRender)
		{
			if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
				if (std::shared_ptr<RHITextureCube> pc = ShadowPass->GetPointShadowCube())
					pointShadowSrv = std::move(pc);
		}
		RHIContext.RHISetShaderTexture(SF_Pixel, 10, pointShadowSrv);

		std::shared_ptr<RHITexture2D> spotShadowSrv = FallbackBrdfLut;
		if (SpotShadowUniform && SpotShadowUniform->GetRHIBuffer() && SpotShadowUniform->Data.SpotShadowEnabled != 0 && WorldSceneRender)
		{
			if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
				if (const std::shared_ptr<RHIRenderTarget> srt = ShadowPass->GetSpotShadowMap())
					if (std::shared_ptr<RHITexture2D> st = srt->GetTex())
						spotShadowSrv = std::move(st);
		}
		RHIContext.RHISetShaderTexture(SF_Pixel, 11, spotShadowSrv);

		std::shared_ptr<RHITexture2D> groundEnvSrv = FallbackBrdfLut;
		if (SkyLightIBL)
			if (std::shared_ptr<RHITexture2D> gt = SkyLightIBL->GetGroundHemiIBLLatLong())
				groundEnvSrv = std::move(gt);
		RHIContext.RHISetShaderTexture(SF_Pixel, 12, groundEnvSrv);

		RHIContext.Draw(3);
	}

	void DeferredLightingPass::Execute(RHICommandContext& RHIContext, std::shared_ptr<RHIViewPort> ViewPort, const std::shared_ptr<FSceneTextures>& SceneTextures,
									   FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData) const
	{
		CopySceneColorToPreLighting(RHIContext, SceneTextures);
		ExecuteRaster(RHIContext, std::move(ViewPort), SceneTextures, WorldSceneRender, ViewData);
	}

	void DeferredLightingPass::BindFurForwardSharedSRVs(RHICommandContext& RHIContext, const std::shared_ptr<FSceneTextures>& SceneTextures,
														FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData) const
	{
		if (!SceneTextures || !ViewData)
			return;
		USkyLightComponent* SkyLightIBL = WorldSceneRender ? WorldSceneRender->GetUSkyLightComponent().get() : nullptr;
		if (PerFrameUniform && PerFrameUniform->GetRHIBuffer())
		{
			FillPerFrameFromView(*PerFrameUniform, PointShadowUniform ? PointShadowUniform.get() : nullptr, SpotShadowUniform ? SpotShadowUniform.get() : nullptr,
								 DirectionalShadowUniform ? DirectionalShadowUniform.get() : nullptr, *ViewData, SkyLightIBL, WorldSceneRender);
			if (PointShadowUniform && PointShadowUniform->GetRHIBuffer())
				RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, *PointShadowUniform);
			if (SpotShadowUniform && SpotShadowUniform->GetRHIBuffer())
				RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, *SpotShadowUniform);
			if (DirectionalShadowUniform && DirectionalShadowUniform->GetRHIBuffer())
				RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, *DirectionalShadowUniform);
		}

		RHIContext.RHISetShaderSampler(SF_Pixel, 0, RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderSampler(SF_Pixel, 1, RHICachedStates::ShadowSampler);
		RHIContext.RHISetShaderSampler(SF_Pixel, 2, RHICachedStates::ShadowCompareSampler);

		std::shared_ptr<RHITextureCube> irrCube = FallbackIBLCube;
		std::shared_ptr<RHITextureCube> specCube = FallbackIBLCube;
		std::shared_ptr<RHITexture2D> brdfLut = FallbackBrdfLut;
		if (SkyLightIBL)
		{
			if (const auto t = SkyLightIBL->GetDiffuseIrradianceCubemap())
				irrCube = t;
			if (const auto t = SkyLightIBL->GetSpecularReflectionCubemap())
				specCube = t;
			if (const auto t = SkyLightIBL->GetBRDFIntegrationLUT())
				brdfLut = t;
		}
		RHIContext.RHISetShaderTexture(SF_Pixel, 5, irrCube);
		RHIContext.RHISetShaderTexture(SF_Pixel, 6, brdfLut);
		RHIContext.RHISetShaderTexture(SF_Pixel, 7, specCube);

		const int32_t pdiFur = PerFrameUniform ? PerFrameUniform->Data.myPerFrame.PrimaryDirectionalLightIndex : -1;
		const bool bDeferredShadow = WorldSceneRender && PerFrameUniform && PerFrameUniform->GetRHIBuffer() && PerFrameUniform->Data.myPerFrame.LightCount > 0 && pdiFur >= 0
			&& PerFrameUniform->Data.myPerFrame.Lights[pdiFur].Type == LightType_Directional
			&& PerFrameUniform->Data.myPerFrame.Lights[pdiFur].ShadowMapIndex >= 0;
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

		std::shared_ptr<RHITextureCube> pointShadowSrv = FallbackIBLCube;
		if (PointShadowUniform && PointShadowUniform->GetRHIBuffer() && PointShadowUniform->Data.Enabled != 0 && WorldSceneRender)
		{
			if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
				if (std::shared_ptr<RHITextureCube> pc = ShadowPass->GetPointShadowCube())
					pointShadowSrv = std::move(pc);
		}
		RHIContext.RHISetShaderTexture(SF_Pixel, 10, pointShadowSrv);

		std::shared_ptr<RHITexture2D> spotShadowSrvFur = FallbackBrdfLut;
		if (SpotShadowUniform && SpotShadowUniform->GetRHIBuffer() && SpotShadowUniform->Data.SpotShadowEnabled != 0 && WorldSceneRender)
		{
			if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
				if (const std::shared_ptr<RHIRenderTarget> srt = ShadowPass->GetSpotShadowMap())
					if (std::shared_ptr<RHITexture2D> st = srt->GetTex())
						spotShadowSrvFur = std::move(st);
		}
		RHIContext.RHISetShaderTexture(SF_Pixel, 11, spotShadowSrvFur);

		std::shared_ptr<RHITexture2D> groundEnvSrvFur = FallbackBrdfLut;
		if (SkyLightIBL)
			if (std::shared_ptr<RHITexture2D> gt = SkyLightIBL->GetGroundHemiIBLLatLong())
				groundEnvSrvFur = std::move(gt);
		RHIContext.RHISetShaderTexture(SF_Pixel, 12, groundEnvSrvFur);

		{
			// SceneLights + cluster outputs share the SRV table with the IBL/shadow textures above; the buffers are owned
			// here so DispatchClusterLightCulling can populate them once per frame, and BindFurForwardSharedSRVs simply
			// rebinds the SRVs on each material draw (translucent + fur both call this helper).
			RHICommandMark ClusterSrvMark(RHIContext, "ClusteredForward_LightSRVs");
			if (SceneLightBuffer)
				RHIContext.RHISetShaderStructuredBuffer(SF_Pixel, kSceneLightsSrvSlot, SceneLightBuffer);
			if (ClusterLightOffsetCountBuffer)
				RHIContext.RHISetShaderStructuredBuffer(SF_Pixel, kClusterLightOffsetCountSrvSlot, ClusterLightOffsetCountBuffer);
			if (ClusterLightIndexListBuffer)
				RHIContext.RHISetShaderStructuredBuffer(SF_Pixel, kClusterLightIndexListSrvSlot, ClusterLightIndexListBuffer);
		}
	}

	void DeferredLightingPass::DispatchClusterLightCulling(RHICommandContext& RHIContext, const std::shared_ptr<const FSceneViewData>& ViewData) const
	{
		if (!RHI || !ViewData)
			return;
		// Skip when the same FSceneViewData identity already drove a build this frame — translucent + fur share the
		// pass, so without this guard the second call would rerun the upload + dispatch over a still-busy ring slot.
		const uintptr_t ViewKey = reinterpret_cast<uintptr_t>(ViewData.get());
		if (ViewKey == SceneLightLastUploadedViewKey && SceneLightBuffer && ClusterLightOffsetCountBuffer && ClusterLightIndexListBuffer && ClusterBuildShader)
			return;

		core::WallSplitTimer Timing;
		const bool bCreateSceneLights = !SceneLightBuffer;
		const bool bCreateOffsetCount = !ClusterLightOffsetCountBuffer;
		const bool bCreateIndexList = !ClusterLightIndexListBuffer;
		const bool bCreateUniform = !ClusterBuildUniform;
		const bool bCreateShader = !ClusterBuildShader;
		const bool bCreatedAnyResource = bCreateSceneLights || bCreateOffsetCount || bCreateIndexList || bCreateUniform || bCreateShader;

		// Lazy create on first use. Lights buffer stays dynamic (CPU-renewed each frame); cluster outputs are static
		// DEFAULT-heap with UAV bind because the CS owns the writes and the PS sees them via SRV in the same frame.
		if (!SceneLightBuffer)
			SceneLightBuffer = RHI->RHICreateStructuredBuffer(sizeof(Light), kSceneLightBufferCapacity, BUF_Dynamic, nullptr);
		if (!ClusterLightOffsetCountBuffer)
			ClusterLightOffsetCountBuffer = RHI->RHICreateStructuredBuffer(
				static_cast<uint32_t>(sizeof(uint32_t) * 2u), ClusterLightCulling::kClusterCount,
				static_cast<EBufferUsageFlags>(BUF_Static | BUF_UnorderedAccess), nullptr);
		if (!ClusterLightIndexListBuffer)
			ClusterLightIndexListBuffer = RHI->RHICreateStructuredBuffer(
				static_cast<uint32_t>(sizeof(uint32_t)), ClusterLightCulling::kClusterCount * ClusterLightCulling::kMaxLightsPerCluster,
				static_cast<EBufferUsageFlags>(BUF_Static | BUF_UnorderedAccess), nullptr);
		if (!ClusterBuildUniform)
			ClusterBuildUniform = std::make_unique<CBClusterBuildWrap>(RHI);
		if (!ClusterBuildShader)
		{
			const std::wstring Path = core::process_directory().wstring() + L"/ShaderLibDX/ClusterLightBuildCS.hlsl";
			ClusterBuildShader = RHI->RHICreateComputeShader(Path, "MainCS", {});
		}
		const double MsCreate = Timing.split_ms();
		if (!SceneLightBuffer || !ClusterLightOffsetCountBuffer || !ClusterLightIndexListBuffer || !ClusterBuildUniform
			|| !ClusterBuildUniform->GetRHIBuffer() || !ClusterBuildShader)
		{
			core::inf() << core::perf::hdr(core::perf::kRenderRec, "ClusteredForwardBuildFailed") << "create_ms=" << MsCreate
						<< " scene_lights=" << (SceneLightBuffer ? 1 : 0)
						<< " offset_count=" << (ClusterLightOffsetCountBuffer ? 1 : 0)
						<< " index_list=" << (ClusterLightIndexListBuffer ? 1 : 0)
						<< " uniform=" << (ClusterBuildUniform && ClusterBuildUniform->GetRHIBuffer() ? 1 : 0)
						<< " shader=" << (ClusterBuildShader ? 1 : 0) << "\n";
			return;
		}

		// Upload the per-view light list once. Both forward translucent / fur and the CS read this buffer; the dynamic
		// ring-buffer in D3D12 advances slot so the GPU still sees the previous frame's payload until the dispatch lands.
		const std::vector<Light>& ViewLights = ViewData->Lights;
		const uint32_t LightCount = (std::min)(static_cast<uint32_t>(ViewLights.size()), kSceneLightBufferCapacity);
		if (LightCount > 0)
			SceneLightBuffer->UpdateStructuredBuffer(ViewLights.data(), LightCount * static_cast<uint32_t>(sizeof(Light)));
		const double MsUploadLights = Timing.split_ms();

		// Build the CS CB. View + proj inverse are split out from CurrViewProjInverseMatrix because the cluster
		// algorithm needs only the projection inverse (NDC -> view) — combining with view would break the math.
		ClusterBuildUniform->Data.ClusterViewMatrix = ViewData->ViewMatrix;
		ClusterBuildUniform->Data.ClusterInvProjMatrix = ViewData->ProjMatrix.Inverse();
		ClusterBuildUniform->Data.ClusterNearZ = ViewData->CameraNearZ;
		ClusterBuildUniform->Data.ClusterFarZ = ViewData->CameraFarZ;
		ClusterBuildUniform->Data.ClusterLightCount = LightCount;
		ClusterBuildUniform->Data.ClusterPad0 = 0u;
		const double MsBuildCB = Timing.split_ms();

		RHICommandMark Mark(RHIContext, "BuildClusteredLights");

		ComputePipelineStateInitializer Init;
		Init.ComputeShader = ClusterBuildShader;
		RHIContext.RHISetComputePipelineState(Init);
		RenderCore::RHI_UpdateAndBindUniformBuffer(RHIContext, *ClusterBuildUniform, SF_Compute);
		RHIContext.RHISetShaderStructuredBuffer(SF_Compute, 0u, SceneLightBuffer);
		RHIContext.RHISetShaderStructuredBufferUAV(0u, ClusterLightOffsetCountBuffer);
		RHIContext.RHISetShaderStructuredBufferUAV(1u, ClusterLightIndexListBuffer);
		const double MsBind = Timing.split_ms();

		constexpr uint32_t ThreadGroupSize = 64u;
		const uint32_t GroupCount = (ClusterLightCulling::kClusterCount + ThreadGroupSize - 1u) / ThreadGroupSize;
		RHIContext.RHIDispatchComputeShader(GroupCount, 1u, 1u);

		// Detach the UAVs after dispatch so the subsequent SetShaderStructuredBuffer(SF_Pixel, ...) can transition the
		// same resources to PIXEL_SHADER_RESOURCE without the D3D11 simultaneous-bind warning.
		RHIContext.RHISetShaderStructuredBufferUAV(0u, std::shared_ptr<RHIStructuredBuffer>{});
		RHIContext.RHISetShaderStructuredBufferUAV(1u, std::shared_ptr<RHIStructuredBuffer>{});
		const double MsDispatchRecord = Timing.split_ms();

		SceneLightLastUploadedViewKey = ViewKey;

		const double MsTotal = Timing.total_ms();
		const uint32_t TimingFrame = ++ClusterTimingLogFrameCounter;
		const bool bVerbose = core::CommandLine::Get().GetName("cluster_timing_verbose");
		const bool bPeriodic = TimingFrame <= 8u || (TimingFrame % 120u) == 0u;
		if (bVerbose || bCreatedAnyResource || bPeriodic || MsTotal >= 1.0)
		{
			core::inf() << core::perf::hdr(core::perf::kRenderRec, "ClusteredForwardBuild") << "wall_ms=" << MsTotal
						<< " create_ms=" << MsCreate
						<< " upload_lights_ms=" << MsUploadLights
						<< " build_cb_ms=" << MsBuildCB
						<< " bind_ms=" << MsBind
						<< " dispatch_record_ms=" << MsDispatchRecord
						<< " lights=" << LightCount
						<< " source_lights=" << static_cast<uint32_t>(ViewLights.size())
						<< " clusters=" << ClusterLightCulling::kClusterCount
						<< " max_lights_per_cluster=" << ClusterLightCulling::kMaxLightsPerCluster
						<< " groups=" << GroupCount
						<< " created_scene_lights=" << (bCreateSceneLights ? 1 : 0)
						<< " created_offset_count=" << (bCreateOffsetCount ? 1 : 0)
						<< " created_index_list=" << (bCreateIndexList ? 1 : 0)
						<< " created_uniform=" << (bCreateUniform ? 1 : 0)
						<< " created_shader=" << (bCreateShader ? 1 : 0) << "\n";
		}
	}
}
