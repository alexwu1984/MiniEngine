#include "Render/SceneRendering/DeferredLightingPass.h"
#include "Render/RDGUtils.h"
#include "Render/SceneRendering/RDGDeferredLightingPass.h"
#include "RHI/RHIRenderPass.h"
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
#include "D3D12/D3D12RHIRecording.h"
#include "core/color.h"
#include "core/commandline.h"
#include "core/logger.h"
#include "core/system.h"
#include "core/wall_timer.h"

namespace Engine
{
	using namespace RenderCore;

	std::vector<FRDGPassResource> GatherFurForwardSharedTwoDimensionalSrvInputs(const FFurForwardSharedSrvSet& S)
	{
		using A = FRDGResourceAccess;
		// Capture shared_ptrs by value: callers may fill OmBarrier.Inputs inside a nested scope and append barriers later.
		return {
			{"BrdfLut", [brdf = S.BrdfLut]() { return brdf; }, true, A::SRV},
			{"DirectionalShadow", [dirShadow = S.DirectionalShadow]() { return dirShadow; }, true, A::SRV},
			{"SpotShadow", [spotShadow = S.SpotShadow]() { return spotShadow; }, true, A::SRV},
			{"GroundEnvLatLong", [ground = S.GroundEnvLatLong]() { return ground; }, true, A::SRV},
		};
	}

	void AppendFurForwardSharedCubeTextureBarriers(std::vector<RenderCore::FRDGTextureBarrierDesc>& Out, const FFurForwardSharedSrvSet& S)
	{
		auto pushCube = [&Out](const std::shared_ptr<RenderCore::RHITextureCube>& Cube) {
			if (!Cube)
				return;
			RenderCore::FRDGTextureBarrierDesc B;
			B.TextureCube = Cube;
			B.Access = RenderCore::FRDGResourceAccess::SRV;
			Out.push_back(std::move(B));
		};
		pushCube(S.IrradianceCube);
		pushCube(S.SpecularCube);
		pushCube(S.PointShadowCube);
	}

	void AppendFurForwardSharedStructuredBufferPixelSrvBarriers(std::vector<RenderCore::FRDGStructuredBufferBarrierDesc>& Out, const FFurForwardSharedSrvSet& S)
	{
		auto pushSrv = [&Out](const std::shared_ptr<RenderCore::RHIStructuredBuffer>& Buf) {
			if (!Buf)
				return;
			Out.push_back({ Buf, RenderCore::FRDGResourceAccess::SRV, false });
		};
		pushSrv(S.SceneLights);
		pushSrv(S.ClusterLightOffsetCount);
		pushSrv(S.ClusterLightIndexList);
	}

	namespace
	{
		// Capacity for StructuredBuffer<Light> (clustered forward PS reads slot 13).
		constexpr uint32_t kSceneLightBufferCapacity = 256u;

		constexpr uint32_t kSceneLightsSrvSlot = 13u;
		/** Pixel slots for cluster buffers (matches ClusterLightLookup.hlsl). */
		constexpr uint32_t kClusterLightOffsetCountSrvSlot = 14u;
		constexpr uint32_t kClusterLightIndexListSrvSlot = 15u;

		constexpr int32_t kFallbackPointShadowCubeSize = 8;

		/** PF_ShadowDepth 2D from FallbackCompareShadowDepthRt, else IfMissing (creation failure only — breaks SampleCmp slots). */
		std::shared_ptr<RHITexture2D> TexFromCompareShadowDepthFallback(const std::shared_ptr<RHIRenderTarget>& DepthRt,
																		const std::shared_ptr<RHITexture2D>& IfMissing)
		{
			if (DepthRt)
				if (std::shared_ptr<RHITexture2D> T = DepthRt->GetTex())
					return T;
			return IfMissing;
		}

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
			Out.Data.myPerFrame.CameraWorldToView = View.ViewMatrix;
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
		// SampleCmp fallbacks: t8 + t11 share one 1×1 depth RT; t10 uses cube depth (see header block comment).
		if (RHI && !FallbackCompareShadowDepthRt)
		{
			FallbackCompareShadowDepthRt = RHI->RHICreateRenderTarget(EPixelFormat::PF_ShadowDepth, 1, 1, 1, false, false);
			if (FallbackCompareShadowDepthRt && RHI->GetDefaultCommandContext())
			{
				const D3D12RHI_ScopedRecordingContext ScopedOutsideFrame(ERHIRecordingContextScope::OutsideFrameResourceUpload);
				RHI->GetDefaultCommandContext()->Clear(FallbackCompareShadowDepthRt, core::FLinearColor::White, 1.f, 0);
			}
		}
		if (RHI && !FallbackPointShadowCube)
		{
			FallbackPointShadowCube =
				RHI->RHICreateTextureCube(EPixelFormat::PF_ShadowDepth, kFallbackPointShadowCubeSize, kFallbackPointShadowCubeSize, 1, false);
			if (FallbackPointShadowCube)
				if (const std::shared_ptr<RHICommandContext> InitCtx = RHI->GetDefaultCommandContext())
				{
					const D3D12RHI_ScopedRecordingContext ScopedOutsideFrame(ERHIRecordingContextScope::OutsideFrameResourceUpload);
					for (int face = 0; face < 6; ++face)
						InitCtx->Clear(FallbackPointShadowCube, face, 0, core::FLinearColor::White, 1.f, 0);
				}
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
		if (core::perf::ShouldEmitPerfInfLogs())
		{
			core::inf() << core::perf::hdr(core::perf::kShaderJit, "DeferredLightingInit") << "wall_ms=" << MsBuffersWall << " fallback_tex_ms=" << MsFallbackTex
						<< " uniform_buffers_ms=" << MsUniforms << " total_ms=" << MsTotal
						<< " note=screen_quad_vs_ps_jit_on_first_ExecuteRaster_Perf|shader_jit|DeferredLightingJitShaders\n";
		}
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
		if ((bVerboseJit || MsJitWall >= kPerfShaderJitLogMinWallMs) && core::perf::ShouldEmitPerfInfLogs())
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

		// D3D11: ClusterLightBuildCS binds SceneLightBuffer on SF_Compute SRV slot 0; UAV slots are cleared after dispatch but CS SRV remains.
		// Binding the same buffer as PS structured SRV (t13) while CS still references it triggers SDKLayers hazards at Draw().
		RHIContext.RHISetShaderStructuredBuffer(SF_Compute, 0u, nullptr);

		USkyLightComponent* SkyLightIBL = nullptr;
		if (WorldSceneRender)
			SkyLightIBL = WorldSceneRender->GetUSkyLightComponent().get();

		FillPerFrameFromView(*PerFrameUniform, PointShadowUniform ? PointShadowUniform.get() : nullptr, SpotShadowUniform ? SpotShadowUniform.get() : nullptr,
							 DirectionalShadowUniform ? DirectionalShadowUniform.get() : nullptr, *ViewData, SkyLightIBL, WorldSceneRender);

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

		const int32_t pdi = PerFrameUniform->Data.myPerFrame.PrimaryDirectionalLightIndex;
		const bool bDeferredShadow = WorldSceneRender && PerFrameUniform->Data.myPerFrame.LightCount > 0 && pdi >= 0
			&& PerFrameUniform->Data.myPerFrame.Lights[pdi].Type == LightType_Directional
			&& PerFrameUniform->Data.myPerFrame.Lights[pdi].ShadowMapIndex >= 0;

		std::shared_ptr<RHITexture2D> shadowSrvTex = TexFromCompareShadowDepthFallback(FallbackCompareShadowDepthRt, FallbackBrdfLut);
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

		std::shared_ptr<RHITexture2D> materialAuxSrv = FallbackBrdfLut;
		if (std::shared_ptr<RHITexture2D> ma = SceneTextures->GetMaterialAuxBuffer())
			materialAuxSrv = std::move(ma);

		std::shared_ptr<RHITextureCube> pointShadowSrv = FallbackPointShadowCube ? FallbackPointShadowCube : FallbackIBLCube;
		if (PointShadowUniform && PointShadowUniform->GetRHIBuffer() && PointShadowUniform->Data.Enabled != 0 && WorldSceneRender)
		{
			if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
				if (std::shared_ptr<RHITextureCube> pc = ShadowPass->GetPointShadowCube())
					pointShadowSrv = std::move(pc);
		}

		std::shared_ptr<RHITexture2D> spotShadowSrv = TexFromCompareShadowDepthFallback(FallbackCompareShadowDepthRt, FallbackBrdfLut);
		if (SpotShadowUniform && SpotShadowUniform->GetRHIBuffer() && SpotShadowUniform->Data.SpotShadowEnabled != 0 && WorldSceneRender)
		{
			if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
				if (const std::shared_ptr<RHIRenderTarget> srt = ShadowPass->GetSpotShadowMap())
					if (std::shared_ptr<RHITexture2D> st = srt->GetTex())
						spotShadowSrv = std::move(st);
		}

		std::shared_ptr<RHITexture2D> groundEnvSrv = FallbackBrdfLut;
		if (SkyLightIBL)
			if (std::shared_ptr<RHITexture2D> gt = SkyLightIBL->GetGroundHemiIBLLatLong())
				groundEnvSrv = std::move(gt);

		RenderCore::FRHIRenderPassDesc RasterOmDesc = RenderCore::FRHIRenderPassDesc::SingleColorNoDepth(SceneColor);
		RasterOmDesc.DebugName = "DeferredLighting_RasterOM";
		{
			FRDGPassDescriptor RasterBarrierSrc{};
			RasterBarrierSrc.Inputs = FRDGDeferredLightingPass::GatherRasterPassInputs(SceneTextures);
			RasterBarrierSrc.Outputs = FRDGDeferredLightingPass::GatherRasterPassOutputs(SceneTextures);
			FRDGUtils::AppendPassTextureBarriers(RasterBarrierSrc, RasterOmDesc.DeclaredTextureBarriers);
		}
		FRDGUtils::AppendDeclaredTextureCubeSrv(RasterOmDesc.DeclaredTextureBarriers, irrCube);
		FRDGUtils::AppendDeclaredTextureCubeSrv(RasterOmDesc.DeclaredTextureBarriers, specCube);
		FRDGUtils::AppendDeclaredTexture2DSrv(RasterOmDesc.DeclaredTextureBarriers, brdfLut);
		FRDGUtils::AppendDeclaredTexture2DSrv(RasterOmDesc.DeclaredTextureBarriers, shadowSrvTex);
		FRDGUtils::AppendDeclaredTexture2DSrv(RasterOmDesc.DeclaredTextureBarriers, materialAuxSrv);
		FRDGUtils::AppendDeclaredTextureCubeSrv(RasterOmDesc.DeclaredTextureBarriers, pointShadowSrv);
		FRDGUtils::AppendDeclaredTexture2DSrv(RasterOmDesc.DeclaredTextureBarriers, spotShadowSrv);
		FRDGUtils::AppendDeclaredTexture2DSrv(RasterOmDesc.DeclaredTextureBarriers, groundEnvSrv);

		if (ClusterLightOffsetCountBuffer)
			RasterOmDesc.DeclaredStructuredBufferBarriers.push_back({ ClusterLightOffsetCountBuffer, FRDGResourceAccess::SRV, false });
		if (ClusterLightIndexListBuffer)
			RasterOmDesc.DeclaredStructuredBufferBarriers.push_back({ ClusterLightIndexListBuffer, FRDGResourceAccess::SRV, false });

		RenderCore::FRHIRenderPassScope RasterOmScope(RHIContext, std::move(RasterOmDesc));

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

		RHIContext.RHISetShaderTexture(SF_Pixel, 5, irrCube);
		RHIContext.RHISetShaderTexture(SF_Pixel, 6, brdfLut);
		RHIContext.RHISetShaderTexture(SF_Pixel, 7, specCube);

		RHIContext.RHISetShaderTexture(SF_Pixel, 8, shadowSrvTex);

		RHIContext.RHISetShaderTexture(SF_Pixel, 9, materialAuxSrv);

		RHIContext.RHISetShaderTexture(SF_Pixel, 10, pointShadowSrv);

		RHIContext.RHISetShaderTexture(SF_Pixel, 11, spotShadowSrv);

		RHIContext.RHISetShaderTexture(SF_Pixel, 12, groundEnvSrv);

		// Same cluster SRV layout as forward (`ClusterLightLookup.hlsl` t13-t15).
		if (SceneLightBuffer)
			RHIContext.RHISetShaderStructuredBuffer(SF_Pixel, kSceneLightsSrvSlot, SceneLightBuffer);
		if (ClusterLightOffsetCountBuffer)
			RHIContext.RHISetShaderStructuredBuffer(SF_Pixel, kClusterLightOffsetCountSrvSlot, ClusterLightOffsetCountBuffer);
		if (ClusterLightIndexListBuffer)
			RHIContext.RHISetShaderStructuredBuffer(SF_Pixel, kClusterLightIndexListSrvSlot, ClusterLightIndexListBuffer);

		RHIContext.Draw(3);
	}

	void DeferredLightingPass::Execute(RHICommandContext& RHIContext, std::shared_ptr<RHIViewPort> ViewPort, const std::shared_ptr<FSceneTextures>& SceneTextures,
									   FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData) const
	{
		CopySceneColorToPreLighting(RHIContext, SceneTextures);
		ExecuteRaster(RHIContext, std::move(ViewPort), SceneTextures, WorldSceneRender, ViewData);
	}

	void GatherFurForwardSharedSrvSet(const DeferredLightingPass& Pass, FWorldSceneRender* WorldSceneRender, const FSceneViewData& ViewData,
									  FFurForwardSharedSrvSet& Out)
	{
		USkyLightComponent* SkyLightIBL = WorldSceneRender ? WorldSceneRender->GetUSkyLightComponent().get() : nullptr;
		Out.IrradianceCube = Pass.FallbackIBLCube;
		Out.SpecularCube = Pass.FallbackIBLCube;
		Out.BrdfLut = Pass.FallbackBrdfLut;
		Out.DirectionalShadow = TexFromCompareShadowDepthFallback(Pass.FallbackCompareShadowDepthRt, Pass.FallbackBrdfLut);
		Out.PointShadowCube = Pass.FallbackPointShadowCube ? Pass.FallbackPointShadowCube : Pass.FallbackIBLCube;
		Out.SpotShadow = TexFromCompareShadowDepthFallback(Pass.FallbackCompareShadowDepthRt, Pass.FallbackBrdfLut);
		Out.GroundEnvLatLong = Pass.FallbackBrdfLut;
		Out.SceneLights = Pass.SceneLightBuffer;
		Out.ClusterLightOffsetCount = Pass.ClusterLightOffsetCountBuffer;
		Out.ClusterLightIndexList = Pass.ClusterLightIndexListBuffer;

		if (SkyLightIBL)
		{
			if (const auto t = SkyLightIBL->GetDiffuseIrradianceCubemap())
				Out.IrradianceCube = t;
			if (const auto t = SkyLightIBL->GetSpecularReflectionCubemap())
				Out.SpecularCube = t;
			if (const auto t = SkyLightIBL->GetBRDFIntegrationLUT())
				Out.BrdfLut = t;
			if (std::shared_ptr<RHITexture2D> gt = SkyLightIBL->GetGroundHemiIBLLatLong())
				Out.GroundEnvLatLong = std::move(gt);
		}

		const int32_t pdiFur = Pass.PerFrameUniform ? Pass.PerFrameUniform->Data.myPerFrame.PrimaryDirectionalLightIndex : -1;
		const bool bDeferredShadow = WorldSceneRender && Pass.PerFrameUniform && Pass.PerFrameUniform->GetRHIBuffer() && Pass.PerFrameUniform->Data.myPerFrame.LightCount > 0 && pdiFur >= 0
			&& Pass.PerFrameUniform->Data.myPerFrame.Lights[pdiFur].Type == LightType_Directional
			&& Pass.PerFrameUniform->Data.myPerFrame.Lights[pdiFur].ShadowMapIndex >= 0;
		if (bDeferredShadow)
		{
			if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
				if (const std::shared_ptr<RHIRenderTarget> shadowRt = ShadowPass->GetShadowMap())
					if (std::shared_ptr<RHITexture2D> st = shadowRt->GetTex())
						Out.DirectionalShadow = std::move(st);
		}

		if (Pass.PointShadowUniform && Pass.PointShadowUniform->GetRHIBuffer() && Pass.PointShadowUniform->Data.Enabled != 0 && WorldSceneRender)
		{
			if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
				if (std::shared_ptr<RHITextureCube> pc = ShadowPass->GetPointShadowCube())
					Out.PointShadowCube = std::move(pc);
		}

		if (Pass.SpotShadowUniform && Pass.SpotShadowUniform->GetRHIBuffer() && Pass.SpotShadowUniform->Data.SpotShadowEnabled != 0 && WorldSceneRender)
		{
			if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
				if (const std::shared_ptr<RHIRenderTarget> srt = ShadowPass->GetSpotShadowMap())
					if (std::shared_ptr<RHITexture2D> st = srt->GetTex())
						Out.SpotShadow = std::move(st);
		}
	}

	namespace FurForwardSharedSrvDetail
	{
		static void BindFurForwardSharedSrvSet(RHICommandContext& RHIContext, const FFurForwardSharedSrvSet& Srvs)
		{
			RHIContext.RHISetShaderTexture(SF_Pixel, 5, Srvs.IrradianceCube);
			RHIContext.RHISetShaderTexture(SF_Pixel, 6, Srvs.BrdfLut);
			RHIContext.RHISetShaderTexture(SF_Pixel, 7, Srvs.SpecularCube);
			RHIContext.RHISetShaderTexture(SF_Pixel, 8, Srvs.DirectionalShadow);
			RHIContext.RHISetShaderTexture(SF_Pixel, 10, Srvs.PointShadowCube);
			RHIContext.RHISetShaderTexture(SF_Pixel, 11, Srvs.SpotShadow);
			RHIContext.RHISetShaderTexture(SF_Pixel, 12, Srvs.GroundEnvLatLong);
			if (Srvs.SceneLights)
				RHIContext.RHISetShaderStructuredBuffer(SF_Pixel, kSceneLightsSrvSlot, Srvs.SceneLights);
			if (Srvs.ClusterLightOffsetCount)
				RHIContext.RHISetShaderStructuredBuffer(SF_Pixel, kClusterLightOffsetCountSrvSlot, Srvs.ClusterLightOffsetCount);
			if (Srvs.ClusterLightIndexList)
				RHIContext.RHISetShaderStructuredBuffer(SF_Pixel, kClusterLightIndexListSrvSlot, Srvs.ClusterLightIndexList);
		}
	} // namespace FurForwardSharedSrvDetail

	void DeferredLightingPass::PrepareForwardSharedSrvSet(FWorldSceneRender* WorldSceneRender,
														  const std::shared_ptr<const FSceneViewData>& ViewData,
														  FFurForwardSharedSrvSet& OutSrvs) const
	{
		OutSrvs = {};
		if (!ViewData)
			return;
		USkyLightComponent* SkyLightIBL = WorldSceneRender ? WorldSceneRender->GetUSkyLightComponent().get() : nullptr;
		if (PerFrameUniform && PerFrameUniform->GetRHIBuffer())
		{
			FillPerFrameFromView(*PerFrameUniform, PointShadowUniform ? PointShadowUniform.get() : nullptr,
								 SpotShadowUniform ? SpotShadowUniform.get() : nullptr,
								 DirectionalShadowUniform ? DirectionalShadowUniform.get() : nullptr, *ViewData, SkyLightIBL, WorldSceneRender);
		}
		GatherFurForwardSharedSrvSet(*this, WorldSceneRender, *ViewData, OutSrvs);
	}

	void DeferredLightingPass::BindFurForwardSharedSRVs(RHICommandContext& RHIContext, const std::shared_ptr<FSceneTextures>& SceneTextures,
														FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData) const
	{
		if (!SceneTextures || !ViewData)
			return;
		FFurForwardSharedSrvSet Srvs;
		PrepareForwardSharedSrvSet(WorldSceneRender, ViewData, Srvs);

		if (PerFrameUniform && PerFrameUniform->GetRHIBuffer())
		{
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

		{
			RHICommandMark ClusterSrvMark(RHIContext, "ClusteredForward_LightSRVs");
			FurForwardSharedSrvDetail::BindFurForwardSharedSrvSet(RHIContext, Srvs);
		}
	}

	void DeferredLightingPass::DispatchClusterLightCulling(RHICommandContext& RHIContext, const std::shared_ptr<const FSceneViewData>& ViewData) const
	{
		if (!RHI || !ViewData)
			return;
		// Idempotent per ViewData pointer (translucent + fur share this path).
		const uintptr_t ViewKey = reinterpret_cast<uintptr_t>(ViewData.get());
		if (ViewKey == SceneLightLastUploadedViewKey && SceneLightBuffer && ClusterLightOffsetCountBuffer && ClusterLightIndexListBuffer && ClusterBuildShader)
			return;

		core::WallSplitTimer Timing;
		const bool bCreateSceneLights = !SceneLightBuffer;
		const bool bCreateOffsetCount = !ClusterLightOffsetCountBuffer;
		const bool bCreateIndexList = !ClusterLightIndexListBuffer;
		const bool bCreateUniform = !ClusterBuildUniform;
		const bool bCreateClusterShader = !ClusterBuildShader;
		const bool bCreatedAnyResource = bCreateSceneLights || bCreateOffsetCount || bCreateIndexList || bCreateUniform || bCreateClusterShader;

		// Lazy alloc; lights buffer dynamic, cluster outputs are UAV static buffers.
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
			if (core::perf::ShouldEmitPerfInfLogs())
			{
				core::inf() << core::perf::hdr(core::perf::kRenderRec, "ClusteredForwardBuildFailed") << "create_ms=" << MsCreate
							<< " scene_lights=" << (SceneLightBuffer ? 1 : 0)
							<< " offset_count=" << (ClusterLightOffsetCountBuffer ? 1 : 0)
							<< " index_list=" << (ClusterLightIndexListBuffer ? 1 : 0)
							<< " uniform=" << (ClusterBuildUniform && ClusterBuildUniform->GetRHIBuffer() ? 1 : 0)
							<< " cluster_cs=" << (ClusterBuildShader ? 1 : 0) << "\n";
			}
			return;
		}

		// Upload lights for this ViewKey (ring slot may still expose prior frame until dispatch completes).
		const std::vector<Light>& ViewLights = ViewData->Lights;
		const uint32_t LightCount = (std::min)(static_cast<uint32_t>(ViewLights.size()), kSceneLightBufferCapacity);
		if (LightCount > 0)
			SceneLightBuffer->UpdateStructuredBuffer(ViewLights.data(), LightCount * static_cast<uint32_t>(sizeof(Light)));
		const double MsUploadLights = Timing.split_ms();

		// Cluster CB uses InvProj for NDC→view only (do not bake view.inverse here).
		ClusterBuildUniform->Data.ClusterViewMatrix = ViewData->ViewMatrix;
		ClusterBuildUniform->Data.ClusterInvProjMatrix = ViewData->ProjMatrix.Inverse();
		ClusterBuildUniform->Data.ClusterNearZ = ViewData->CameraNearZ;
		ClusterBuildUniform->Data.ClusterFarZ = ViewData->CameraFarZ;
		ClusterBuildUniform->Data.ClusterLightCount = LightCount;
		ClusterBuildUniform->Data.ClusterPad0 = 0u;
		const double MsBuildCB = Timing.split_ms();

		RHICommandMark Mark(RHIContext, "BuildGpuLightLists");

		{
			std::vector<FRDGStructuredBufferBarrierDesc> ClusterBeginBarriers;
			ClusterBeginBarriers.push_back({ SceneLightBuffer, FRDGResourceAccess::SRV, true });
			ClusterBeginBarriers.push_back({ ClusterLightOffsetCountBuffer, FRDGResourceAccess::UAV, false });
			ClusterBeginBarriers.push_back({ ClusterLightIndexListBuffer, FRDGResourceAccess::UAV, false });
			RHIContext.RHIRenderPassApplyDeclaredStructuredBufferBarriers(ClusterBeginBarriers.data(), ClusterBeginBarriers.size(), ERDGPassQueue::Graphics);
		}

		ComputePipelineStateInitializer InitCluster;
		InitCluster.ComputeShader = ClusterBuildShader;
		RHIContext.RHISetComputePipelineState(InitCluster);
		RenderCore::RHI_UpdateAndBindUniformBuffer(RHIContext, *ClusterBuildUniform, SF_Compute);
		RHIContext.RHISetShaderStructuredBuffer(SF_Compute, 0u, SceneLightBuffer);
		RHIContext.RHISetShaderStructuredBufferUAV(0u, ClusterLightOffsetCountBuffer);
		RHIContext.RHISetShaderStructuredBufferUAV(1u, ClusterLightIndexListBuffer);
		const double MsBindCluster = Timing.split_ms();

		constexpr uint32_t ThreadGroupSize = 64u;
		const uint32_t GroupCountCluster = (ClusterLightCulling::kClusterCount + ThreadGroupSize - 1u) / ThreadGroupSize;
		RHIContext.RHIDispatchComputeShader(GroupCountCluster, 1u, 1u);

		RHIContext.RHISetShaderStructuredBufferUAV(0u, std::shared_ptr<RHIStructuredBuffer>{});
		RHIContext.RHISetShaderStructuredBufferUAV(1u, std::shared_ptr<RHIStructuredBuffer>{});
		RHIContext.RHISetShaderStructuredBuffer(SF_Compute, 0u, nullptr);
		const double MsDispatchCluster = Timing.split_ms();

		SceneLightLastUploadedViewKey = ViewKey;

		const double MsTotal = Timing.total_ms();
		const uint32_t TimingFrame = ++ClusterTimingLogFrameCounter;
		const bool bVerbose = core::CommandLine::Get().GetName("cluster_timing_verbose");
		const bool bPeriodic = TimingFrame <= 8u || (TimingFrame % 120u) == 0u;
		if ((bVerbose || bCreatedAnyResource || bPeriodic || MsTotal >= 1.0) && core::perf::ShouldEmitPerfInfLogs())
		{
			core::inf() << core::perf::hdr(core::perf::kRenderRec, "ClusteredForwardBuild") << "wall_ms=" << MsTotal
						<< " create_ms=" << MsCreate
						<< " upload_lights_ms=" << MsUploadLights
						<< " build_cb_ms=" << MsBuildCB
						<< " bind_cluster_ms=" << MsBindCluster
						<< " dispatch_cluster_ms=" << MsDispatchCluster
						<< " lights=" << LightCount
						<< " source_lights=" << static_cast<uint32_t>(ViewLights.size())
						<< " clusters=" << ClusterLightCulling::kClusterCount
						<< " max_lights_per_cluster=" << ClusterLightCulling::kMaxLightsPerCluster
						<< " cluster_groups=" << GroupCountCluster
						<< " created_scene_lights=" << (bCreateSceneLights ? 1 : 0)
						<< " created_offset_count=" << (bCreateOffsetCount ? 1 : 0)
						<< " created_index_list=" << (bCreateIndexList ? 1 : 0)
						<< " created_uniform=" << (bCreateUniform ? 1 : 0)
						<< " created_cluster_cs=" << (bCreateClusterShader ? 1 : 0) << "\n";
		}
	}
}
