#include "Render/SceneRendering/SceneRenderer.h"
#include "core/logger.h"
#include "Render/WorldSceneRender.h"
#include "Render/WorldSceneRenderPrivate.h"
#include "Scene/FScene.h"
#include "Render/SkyLightEnvironment.h"
#include "Render/PostProcessor.h"
#include "Render/SkyLightRenderPass.h"
#include "Render/SceneTextures.h"
#include "Render/RDGBuilder.h"
#include "Render/SceneRendering/RDGDeferredLightingPass.h"
#include "Render/SceneRendering/DeferredLightingPass.h"
#include "Render/SceneRendering/DeferredShadingBasePassRenderer.h"
#include "Render/SceneRendering/MeshMaterialRenderCache.h"
#include "Render/SceneRendering/DeferredBasePassDrawContext.h"
#include "Render/MaterialPreFrame.h"
#include "Render/Shadow/ShadowRenderPass.h"
#include "Render/ShadowDebugWireRenderer.h"
#include "Scene/World.h"
#include "Scene/Actor.h"
#include "Scene/DirectionalLightComponent.h"
#include "Scene/PointLightComponent.h"
#include "Scene/SpotLightComponent.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIViewPort.h"
#include "RHI/RHICommandContext.h"
#include "D3D12/D3D12RHIRecording.h"
#include "Engine.h"
#include "App/AppWindow.h"

using namespace RenderCore;

namespace Engine
{
	namespace
	{
		static int IndexOfFirstLightOfType(const std::vector<Light>& lights, int type)
		{
			for (int li = 0; li < static_cast<int>(lights.size()); ++li)
			{
				if (lights[static_cast<size_t>(li)].Type == type)
					return li;
			}
			return -1;
		}

		std::vector<FRDGPassResource> GatherSceneTexturesPassResources(const std::shared_ptr<FSceneTextures>& SceneTextures)
		{
			if (!SceneTextures)
				return {};
			using A = FRDGResourceAccess;
			return {
				{ "SceneColor", [SceneTextures]() { return SceneTextures->GetSceneColor(); }, true, A::RTV },
				{ "MotionVector", [SceneTextures]() { return SceneTextures->GetMotionVector(); }, true, A::RTV },
				{ "Normal", [SceneTextures]() { return SceneTextures->GetNormalBuffer(); }, true, A::RTV },
				{ "Emissive", [SceneTextures]() { return SceneTextures->GetEmissiveBuffer(); }, true, A::RTV },
				{ "MetallicRoughness", [SceneTextures]() { return SceneTextures->GetMetallicRoughnessBuffer(); }, true, A::RTV },
				{ "MaterialAux", [SceneTextures]() { return SceneTextures->GetMaterialAuxBuffer(); }, true, A::RTV },
				{ "Depth", [SceneTextures]() { return SceneTextures->GetDepth(); }, true, A::DSV },
			};
		}
	} // namespace

	void FSceneRenderer::ExecuteFrame(DynamicRHI* RHI, FSceneRenderPacket&& Packet)
	{
		if (!RHI || !Packet.SceneResources || !Packet.ViewData)
			return;

		[[maybe_unused]] const uint64_t SubmissionSequenceForProfiling = Packet.SubmissionSequence;

		FWorldSceneRender* const Self = Packet.WorldSceneRenderOwner;
		FWorldSceneRenderPrivate* const d = Packet.SceneResources;
		std::shared_ptr<const FSceneViewData> ViewConst = std::move(Packet.ViewData);
		std::vector<GltfSceneMeshInfo> MeshesInfoCopy = std::move(Packet.MeshesInfo);
		std::vector<GltfSceneMeshInfo> shadowCasters = std::move(Packet.ShadowCasters);
		std::vector<GltfSceneMeshInfo> shadowFrustumBounds = std::move(Packet.ShadowFrustumBounds);
		std::vector<Light> ShadowPassLights = std::move(Packet.LightsForShadow);
		FShadowProjectorSceneData ShadowProjectorSceneMoved = std::move(Packet.ShadowProjectorScene);
		std::shared_ptr<FScene> WorldSceneForFrame = std::move(Packet.WorldScene);
		FSkyLightSourceDesc SkyLightSrc = std::move(Packet.SkyLightSource);

		std::shared_ptr<RHICommandContext> CommandContext = RHI->GetDefaultCommandContext();
		if (!CommandContext)
			return;

		auto MeshesForDraw = std::make_shared<std::vector<GltfSceneMeshInfo>>(std::move(MeshesInfoCopy));

		d->ShadowDebugSubmit = {};

		bool bAnyShadowLight = false;
		for (const Light& L : ShadowPassLights)
		{
			if (L.ShadowMapIndex >= 0)
			{
				bAnyShadowLight = true;
				break;
			}
		}

		FRDGBuilder Graph;
		auto SceneTextures = d->SceneTextures;
		// RHICreateHDRTexture2D → upload uses D3D12 recording TLS; must run after RHIBeginFrame's RHIFrameBoundary push
		// (Debug ensures; Release no-op check — wrong thread/stack can spiral into device removal / handled _com_error on Present).
		RHI->RHIBeginFrame();
		if (d->SkylightEnvironment)
			d->SkylightEnvironment->ResolveAndApplyHDRSource(SkyLightSrc);
		const RenderCore::D3D12RHI_ScopedRecordingContext ScopedInsideRecordingFrame(
			RenderCore::ERHIRecordingContextScope::InsideFrameTick);

		Graph.AddPass(FRDGPassDescriptor{
			"UpdateSkyLightCaptures",
			{},
			{},
			[d, CommandContext]()
			{
				if (d->SkylightEnvironment)
					d->SkylightEnvironment->Draw(*CommandContext);
			}});

		// Must run when frustumBounds-only receivers exist (no ProjShadow casters) or any light requests a shadow map; otherwise spot/optional dir depth never renders.
		const bool bScheduleShadowPass =
			!shadowCasters.empty() || !shadowFrustumBounds.empty() || ShadowProjectorSceneMoved.bValid || bAnyShadowLight;
		if (bScheduleShadowPass)
		{
			Graph.AddPass(FRDGPassDescriptor{
				"Shadow",
				{},
				{},
				[d, Self, CommandContext, shadowCasters, shadowFrustumBounds, ShadowPassLights = std::move(ShadowPassLights), ShadowProjectorScene = std::move(ShadowProjectorSceneMoved)]() mutable
				{
					d->ShadowRender->Render(shadowCasters, shadowFrustumBounds, *CommandContext, ShadowPassLights, ShadowProjectorScene);

					std::shared_ptr<World> W = Self ? Self->GetWorld() : nullptr;

					Light Dir{};
					int dirLi = -1;
					if (W && d->ShadowRender && d->ShadowRender->TryGetCachedMainLightForShading(Dir, &dirLi) && dirLi >= 0)
					{
						const int firstD = IndexOfFirstLightOfType(ShadowPassLights, LightType_Directional);
						if (firstD >= 0 && dirLi >= firstD)
						{
							const int subDir = dirLi - firstD;
							const auto dirComps = W->GetDirectionalLightsForEditingSorted();
							if (subDir >= 0 && subDir < static_cast<int>(dirComps.size()) && dirComps[static_cast<size_t>(subDir)]
								&& dirComps[static_cast<size_t>(subDir)]->GetShowShadowFrustumDebug())
							{
								if (d->ShadowDebugSubmit.NumDir < FShadowDebugWireSubmit::kMaxDebugLights)
								{
									const auto comp = dirComps[static_cast<size_t>(subDir)];
									const auto owner = comp ? comp->GetOwner() : nullptr;
									FShadowDebugWireSubmit::FDirArrow a{};
									a.Origin = owner ? owner->GetPosition() : math::Vector3(0.f, 0.f, 0.f);
									a.DirectionTowardSource = comp ? comp->GetWorldDirection() : math::Vector3(0.f, 1.f, 0.f);
									a.Length = 2.5f;
									d->ShadowDebugSubmit.Dir[d->ShadowDebugSubmit.NumDir++] = a;
								}
							}
						}
					}

					int spotIdx = -1;
					math::Matrix4x4 spotVp{};
					math::Matrix4x4 spotView{};
					if (W && d->ShadowRender && d->ShadowRender->TryGetCachedSpotShadowForDeferred(spotIdx, spotVp, &spotView) && spotIdx >= 0)
					{
						const int firstS = IndexOfFirstLightOfType(ShadowPassLights, LightType_Spot);
						if (firstS >= 0 && spotIdx >= firstS)
						{
							const int subS = spotIdx - firstS;
							const auto spots = W->GetSpotLightsForEditingSorted();
							if (subS >= 0 && subS < static_cast<int>(spots.size()) && spots[static_cast<size_t>(subS)]
								&& spots[static_cast<size_t>(subS)]->GetShowShadowFrustumDebug())
							{
								if (d->ShadowDebugSubmit.NumSpot < FShadowDebugWireSubmit::kMaxDebugLights)
								{
									const auto comp = spots[static_cast<size_t>(subS)];
									const auto owner = comp ? comp->GetOwner() : nullptr;
									FShadowDebugWireSubmit::FSpotCone c{};
									c.Apex = owner ? owner->GetPosition() : math::Vector3(0.f, 0.f, 0.f);
									c.ConeAxis = comp ? comp->GetConeAxisWorld() : math::Vector3(0.f, 0.f, 1.f);
									c.Range = comp ? comp->GetRange() : 10.f;
									c.OuterConeCos = comp ? comp->GetOuterConeCos() : 0.70710677f;
									d->ShadowDebugSubmit.Spot[d->ShadowDebugSubmit.NumSpot++] = c;
								}
							}
						}
					}

					int pointIdx = -1;
					math::Matrix4x4 pointFaceVp[6]{};
					math::Vector4 pointPosRange{};
					if (W && d->ShadowRender && d->ShadowRender->TryGetCachedPointShadowForDeferred(pointIdx, pointFaceVp, pointPosRange)
						&& pointIdx >= 0)
					{
						const int firstP = IndexOfFirstLightOfType(ShadowPassLights, LightType_Point);
						if (firstP >= 0 && pointIdx >= firstP)
						{
							const int subP = pointIdx - firstP;
							const auto points = W->GetPointLightsForEditingSorted();
							if (subP >= 0 && subP < static_cast<int>(points.size()) && points[static_cast<size_t>(subP)]
								&& points[static_cast<size_t>(subP)]->GetShowShadowFrustumDebug())
							{
								if (d->ShadowDebugSubmit.NumPoint < FShadowDebugWireSubmit::kMaxDebugLights)
								{
									FShadowDebugWireSubmit::FPointSphere s{};
									s.Center = math::Vector3(pointPosRange.x, pointPosRange.y, pointPosRange.z);
									s.Radius = pointPosRange.w;
									d->ShadowDebugSubmit.Point[d->ShadowDebugSubmit.NumPoint++] = s;
								}
							}
						}
					}
				}});
		}
		else if (d->ShadowRender)
		{
			d->ShadowRender->InvalidateCachedMainLightForShading();
		}

		const std::vector<FRDGPassResource> SceneTexturesIO = GatherSceneTexturesPassResources(SceneTextures);
		const bool bDeferTranslucentToForwardPass = d->DeferredLighting && SceneTextures && ViewConst && !ViewConst->bUnlit;

		Graph.AddPass(FRDGPassDescriptor{
			"ClearSceneTextures",
			{},
			SceneTexturesIO,
			[d, CommandContext]()
			{
				// Do not bind/clear the swapchain here: this pass only fills off-screen scene textures. Binding the back buffer
				// then immediately switching to MRT wasted OM state and a full-screen clear before sky/base pass.
				d->MainViewPort->Prepare();
				int32_t width = GEngine->GetAppWindow()->GetWidth();
				int32_t height = GEngine->GetAppWindow()->GetHeight();
				CommandContext->SetViewPort(0, 0, width, height);

				auto DepthTex = d->SceneTextures->GetDepth();
				auto SceneCol = d->SceneTextures->GetSceneColor();
				auto Motion = d->SceneTextures->GetMotionVector();
				auto Emissive = d->SceneTextures->GetEmissiveBuffer();
				auto Normal = d->SceneTextures->GetNormalBuffer();
				auto MR = d->SceneTextures->GetMetallicRoughnessBuffer();
				auto MatAux = d->SceneTextures->GetMaterialAuxBuffer();
				// Black-only clear for motion/emissive/scene is fine. Normal+MR must use neutral dielectric defaults: SrcAlpha-
				// blended fur shells were lerping toward black (ao=0, roughness=0), which zeros IBL diffuse (iblDiffuse*ao) and
				// causes black fringes against the sky.
				CommandContext->Clear(std::vector<std::shared_ptr<RenderCore::RHITexture2D>>{SceneCol, Motion, Emissive}, DepthTex,
									  core::FLinearColor::Black, 1.f, 0);
				CommandContext->Clear(Normal, nullptr, core::FLinearColor(0.5f, 0.5f, 1.f, 0.f), 1.f, 0);
				CommandContext->Clear(MR, nullptr, core::FLinearColor(0.f, 1.f, 0.85f, 1.f), 1.f, 0);
				// Default-lit shading model tag (SHADINGMODELID_DEFAULT_LIT == 1) packed in .r as 1/255; yz Hair tangent unused.
				if (MatAux)
					CommandContext->Clear(MatAux, nullptr, core::FLinearColor(1.f / 255.f, 0.f, 0.f, 0.f), 1.f, 0);
				std::vector<std::shared_ptr<RenderCore::RHITexture2D>> Targets = {SceneCol, Motion, Normal, Emissive, MR};
				// Clear() uses CPU RTV handles only (no OM bind). Establish scene textures as active RTs + depth for subsequent passes.
				CommandContext->SetRenderTarget(Targets, d->SceneTextures->GetDepth());
			}});

		Graph.AddPass(FRDGPassDescriptor{
			"RenderSky",
			SceneTexturesIO,
			SceneTexturesIO,
			[d, CommandContext, ViewConst, SkyLightSrc]()
			{
				std::vector<std::shared_ptr<RenderCore::RHITexture2D>> Targets = {
					d->SceneTextures->GetSceneColor(), d->SceneTextures->GetMotionVector(), d->SceneTextures->GetNormalBuffer(),
					d->SceneTextures->GetEmissiveBuffer(), d->SceneTextures->GetMetallicRoughnessBuffer()};
				if (ViewConst && ViewConst->SkyLightIBLScale > 0.f && d->SkylightEnvironment)
				{
					auto SkyCube = d->SkylightEnvironment->GetSkyLightCubemap();
					d->SkyLightPass->SetTextureCube(SkyCube);
					math::Vector3 sunToward{};
					float sunBloomHdr = 0.f;
					std::shared_ptr<RenderCore::RHITexture2D> groundLatLongDummy;
					float hemiPow = 1.75f;
					float groundInt = 1.f;
					int32_t groundEn = 0;
					if (SkyLightSrc.Type == ESkyLightSourceType::Procedural)
					{
						sunToward = SkyLightSrc.ProceduralSunDirectionTowardSource;
						sunBloomHdr = SkyLightSrc.ProceduralSunBloomLinearHDR;
						if (d->SkylightEnvironment->HasSplitHemisphereGroundIBL())
						{
							groundLatLongDummy = d->SkylightEnvironment->GetGroundHemiIBLLatLong();
							hemiPow = d->SkylightEnvironment->GetHemiIBLBlendPowerForShader();
							groundInt = d->SkylightEnvironment->GetGroundIBLIntensityForShader();
							groundEn = 1;
						}
					}
					if (!groundLatLongDummy)
						groundLatLongDummy = d->SkylightEnvironment->GetBRDFIntegrationLUT();
					d->SkyLightPass->Render(*CommandContext, Targets, d->SceneTextures->GetDepth(), ViewConst->SkyInverseViewProj,
											sunToward, sunBloomHdr, groundLatLongDummy, hemiPow, groundInt, groundEn);
				}
				else
				{
					d->SkyLightPass->SetTextureCube(nullptr);
				}
			}});

		Graph.AddPass(FRDGPassDescriptor{
			"RenderBasePass",
			SceneTexturesIO,
			SceneTexturesIO,
			[d, Self, RHI, CommandContext, MeshesForDraw, ViewConst, WorldSceneForFrame]()
			{
				FMeshMaterialRenderCache* MeshCache = WorldSceneForFrame ? WorldSceneForFrame->GetMeshMaterialRenderCache() : nullptr;
				if (!MeshesForDraw->empty() && MeshCache)
				{
					FDeferredBasePassDrawContext DrawContext;
					DrawContext.ViewData = ViewConst;
					DrawContext.SceneTextures = d->SceneTextures;
					DrawContext.WorldSceneRender = Self;
					DrawContext.RHICmdList = CommandContext.get();
					FDeferredShadingBasePassRenderer::RenderBasePassOpaque(RHI, *MeshesForDraw, DrawContext, *MeshCache);
				}
			}});

		Graph.AddPass(FRDGPassDescriptor{
			"RenderTranslucency",
			SceneTexturesIO,
			SceneTexturesIO,
			[d, Self, RHI, CommandContext, MeshesForDraw, ViewConst, WorldSceneForFrame, bDeferTranslucentToForwardPass]()
			{
				if (bDeferTranslucentToForwardPass)
					return;
				FMeshMaterialRenderCache* MeshCache = WorldSceneForFrame ? WorldSceneForFrame->GetMeshMaterialRenderCache() : nullptr;
				if (!MeshesForDraw->empty() && MeshCache)
				{
					FDeferredBasePassDrawContext DrawContext;
					DrawContext.ViewData = ViewConst;
					DrawContext.SceneTextures = d->SceneTextures;
					DrawContext.WorldSceneRender = Self;
					DrawContext.RHICmdList = CommandContext.get();
					FDeferredShadingBasePassRenderer::RenderBasePassTranslucent(RHI, *MeshesForDraw, DrawContext, *MeshCache);
				}
			}});

		if (d->DeferredLighting && SceneTextures && ViewConst && !ViewConst->bUnlit)
		{
			Graph.AddPass(FRDGPassDescriptor{
				FRDGDeferredLightingPass::PassNameCopySceneToPreLighting,
				FRDGDeferredLightingPass::GatherCopyPassInputs(SceneTextures),
				FRDGDeferredLightingPass::GatherCopyPassOutputs(SceneTextures),
				[d, CommandContext, SceneTextures]()
				{
					if (!d->DeferredLighting)
						return;
					d->DeferredLighting->CopySceneColorToPreLighting(*CommandContext, SceneTextures);
				},
				true,
				RDG_Copy,
				ERDGPassQueue::Graphics,
				true});
			Graph.AddPass(FRDGPassDescriptor{
				FRDGDeferredLightingPass::PassNameRaster,
				FRDGDeferredLightingPass::GatherRasterPassInputs(SceneTextures),
				FRDGDeferredLightingPass::GatherRasterPassOutputs(SceneTextures),
				[d, Self, CommandContext, ViewConst, SceneTextures]()
				{
					if (!d->DeferredLighting || !d->MainViewPort)
						return;
					d->DeferredLighting->ExecuteRaster(*CommandContext, d->MainViewPort, SceneTextures, Self, ViewConst);
				},
				true,
				RDG_Raster,
				ERDGPassQueue::Graphics,
				true});
			Graph.AddPass(FRDGPassDescriptor{
				"RenderTranslucentForward",
				SceneTexturesIO,
				SceneTexturesIO,
				[d, Self, RHI, CommandContext, MeshesForDraw, ViewConst, WorldSceneForFrame]()
				{
					FMeshMaterialRenderCache* MeshCache = WorldSceneForFrame ? WorldSceneForFrame->GetMeshMaterialRenderCache() : nullptr;
					if (!MeshesForDraw->empty() && MeshCache && d->DeferredLighting)
					{
						FDeferredBasePassDrawContext DrawContext;
						DrawContext.ViewData = ViewConst;
						DrawContext.SceneTextures = d->SceneTextures;
						DrawContext.WorldSceneRender = Self;
						DrawContext.RHICmdList = CommandContext.get();
						FDeferredShadingBasePassRenderer::RenderTranslucentForwardAfterDeferredLighting(RHI, *MeshesForDraw, DrawContext, *MeshCache, d->DeferredLighting.get());
					}
				}});
			Graph.AddPass(FRDGPassDescriptor{
				"RenderFurForward",
				SceneTexturesIO,
				SceneTexturesIO,
				[d, Self, RHI, CommandContext, MeshesForDraw, ViewConst, WorldSceneForFrame, SceneTextures]()
				{
					FMeshMaterialRenderCache* MeshCache = WorldSceneForFrame ? WorldSceneForFrame->GetMeshMaterialRenderCache() : nullptr;
					if (!MeshesForDraw->empty() && MeshCache && d->DeferredLighting)
					{
						FDeferredBasePassDrawContext DrawContext;
						DrawContext.ViewData = ViewConst;
						DrawContext.SceneTextures = d->SceneTextures;
						DrawContext.WorldSceneRender = Self;
						DrawContext.RHICmdList = CommandContext.get();
						FDeferredShadingBasePassRenderer::RenderFurForwardAfterDeferredLighting(RHI, *MeshesForDraw, DrawContext, *MeshCache, d->DeferredLighting.get());
					}
				}});
		}

		d->PostProcess->AddFramePasses(Graph, *CommandContext, d->SceneTextures, d->MainViewPort, ViewConst);

		Graph.AddPass(FRDGPassDescriptor{
			"ShadowDebugWire",
			{},
			{},
			[d, CommandContext, ViewConst]()
			{
				if (d->ShadowDebugSubmit.NumDir <= 0 && d->ShadowDebugSubmit.NumSpot <= 0 && d->ShadowDebugSubmit.NumPoint <= 0)
					return;
				if (!d->ShadowDebugWire)
					return;
				FShadowDebugWireSubmit sub = d->ShadowDebugSubmit;
				sub.OverlayWorldToClip = ViewConst->SsrViewProjMatrix;
				d->ShadowDebugWire->Render(*CommandContext, *d->MainViewPort, sub);
			}});

		Graph.AddPass(FRDGPassDescriptor{
			"UIPresent",
			{},
			{},
			[d, Self]()
			{
				Self->sigGuiEvent();
				d->MainViewPort->RHIImGuiRenderDrawData();
				d->MainViewPort->RHISubmitAndPresentFrame();
			},
			false,
			RDG_Raster | RDG_GraphSink,
			ERDGPassQueue::Graphics});

		if (bScheduleShadowPass)
		{
			Graph.AddPassDependency("Shadow", "ClearSceneTextures");
			Graph.AddPassDependency("Shadow", "RenderBasePass");
			Graph.AddPassDependency("Shadow", "RenderTranslucency");
			if (d->DeferredLighting && SceneTextures && ViewConst && !ViewConst->bUnlit)
			{
				Graph.AddPassDependency("Shadow", FRDGDeferredLightingPass::PassNameRaster);
				Graph.AddPassDependency("Shadow", "RenderTranslucentForward");
			}
		}

		if (d->DeferredLighting && SceneTextures && ViewConst && !ViewConst->bUnlit)
		{
			Graph.AddPassDependency(FRDGDeferredLightingPass::PassNameRaster, "RenderTranslucentForward");
			Graph.AddPassDependency("RenderTranslucentForward", "RenderFurForward");
			Graph.AddPassDependency("RenderFurForward", "Tonemapping");
		}

		// After tonemapping: optional shadow debug lines, then ImGui composite and present.
		Graph.AddPassDependency("Tonemapping", "ShadowDebugWire");
		Graph.AddPassDependency("ShadowDebugWire", "UIPresent");

		FRDGCompileParameters RDGExecParams = d->RDGCompileParams;
		RDGExecParams.RDGBarrierCommandContext = CommandContext.get();
		RDGExecParams.PassCpuTimingsOut = &d->ScratchPassCpuTimings;
		if (!Graph.Compile(d->RDGCompileParams, nullptr))
		{
			core::LOG(core::log_err, L"FRDG: frame graph compile failed (cycle); executing passes in AddPass order so Present still runs.");
			Graph.ExecutePassesInSetupOrder(RDGExecParams);
		}
		else
			Graph.ExecutePasses(RDGExecParams);
		{
			std::lock_guard<std::mutex> Lock(d->PassCpuTimingMutex);
			d->LastFramePassCpuTimingsForGui = d->ScratchPassCpuTimings;
		}
		RHI->RHIEndFrame();
	}

} // namespace Engine
