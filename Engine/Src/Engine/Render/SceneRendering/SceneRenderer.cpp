#include "Render/SceneRendering/SceneRenderer.h"
#include "core/logger.h"
#include "Render/WorldSceneRender.h"
#include "Render/WorldSceneRenderPrivate.h"
#include "Render/PreProcessor.h"
#include "Render/IBLRender.h"
#include "Render/PostProcessor.h"
#include "Render/CubeBackground.h"
#include "Render/SceneTextures.h"
#include "Render/RDGBuilder.h"
#include "Render/SceneRendering/RDGDeferredLightingPass.h"
#include "Render/SceneRendering/DeferredLightingPass.h"
#include "Render/SceneRendering/DeferredShadingBasePassRenderer.h"
#include "Render/SceneRendering/MeshMaterialRenderCache.h"
#include "Render/SceneRendering/DeferredBasePassDrawContext.h"
#include "Render/Shadow/ShadowRenderPass.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIViewPort.h"
#include "RHI/RHICommandContext.h"
#include "Engine.h"
#include "App/AppWindow.h"

using namespace RenderCore;

namespace Engine
{
	namespace
	{
		std::vector<FRDGPassResource> GatherSceneTexturesPassResources(const std::shared_ptr<SceneTextures>& TB)
		{
			if (!TB)
				return {};
			using A = FRDGResourceAccess;
			return {
				{ "SceneColor", [TB]() { return TB->GetSceneColor(); }, true, A::RTV },
				{ "MotionVector", [TB]() { return TB->GetMotionVector(); }, true, A::RTV },
				{ "Normal", [TB]() { return TB->GetNormalBuffer(); }, true, A::RTV },
				{ "Emissive", [TB]() { return TB->GetEmissiveBuffer(); }, true, A::RTV },
				{ "MetallicRoughness", [TB]() { return TB->GetMetallicRoughnessBuffer(); }, true, A::RTV },
				{ "MaterialAux", [TB]() { return TB->GetMaterialAuxBuffer(); }, true, A::RTV },
				{ "Depth", [TB]() { return TB->GetDepth(); }, true, A::DSV },
			};
		}
	} // namespace

	void FSceneRenderer::Submit(FWorldSceneRender* InWorldSceneRenderOwner, FWorldSceneRenderPrivate* InSceneResources, const FSceneViewFamily& InViewFamily,
									 std::shared_ptr<const FSceneViewData> InViewData, std::vector<GltfSceneMeshInfo> MeshesInfoCopy,
									 std::vector<GltfSceneMeshInfo> shadowCasters, std::vector<GltfSceneMeshInfo> shadowFrustumBounds,
									 std::vector<Light> ShadowPassLights, FShadowProjectorSceneData InShadowProjectorScene,
									 std::optional<std::wstring> SkyLightHdrFullPathOverride)
	{
		WorldSceneRenderOwner = InWorldSceneRenderOwner;
		SceneResources = InSceneResources;
		ViewFamily = InViewFamily;
		ViewData = std::move(InViewData);
		MeshesInfo = std::move(MeshesInfoCopy);
		ShadowCasters = std::move(shadowCasters);
		ShadowFrustumBounds = std::move(shadowFrustumBounds);
		LightsForShadow = std::move(ShadowPassLights);
		ShadowProjectorScene = InShadowProjectorScene;
		SkyLightHdrOverrideForFrame = std::move(SkyLightHdrFullPathOverride);
		bHasFrame = (WorldSceneRenderOwner != nullptr) && (SceneResources != nullptr) && (ViewData != nullptr);
	}

	void FSceneRenderer::Render(DynamicRHI* RHI)
	{
		if (!bHasFrame || !RHI || !SceneResources || !ViewData)
			return;

		bHasFrame = false;

		FWorldSceneRender* const Self = WorldSceneRenderOwner;
		FWorldSceneRenderPrivate* const d = SceneResources;
		std::shared_ptr<const FSceneViewData> ViewConst = ViewData;
		std::vector<GltfSceneMeshInfo> MeshesInfoCopy = std::move(MeshesInfo);
		std::vector<GltfSceneMeshInfo> shadowCasters = std::move(ShadowCasters);
		std::vector<GltfSceneMeshInfo> shadowFrustumBounds = std::move(ShadowFrustumBounds);
		std::vector<Light> ShadowPassLights = std::move(LightsForShadow);
		FShadowProjectorSceneData ShadowProjectorSceneMoved = ShadowProjectorScene;

		ViewData.reset();
		WorldSceneRenderOwner = nullptr;
		SceneResources = nullptr;

		std::shared_ptr<RHICommandContext> CommandContext = RHI->GetDefaultCommandContext();
		if (!CommandContext)
			return;

		if (d->PreProcess)
			d->PreProcess->ResolveSkyLightForFrame(std::move(SkyLightHdrOverrideForFrame));
		SkyLightHdrOverrideForFrame.reset();

		auto MeshesForDraw = std::make_shared<std::vector<GltfSceneMeshInfo>>(std::move(MeshesInfoCopy));

		FRDGBuilder Graph;
		auto TB = d->TargetBuffer;
		RHI->RHIBeginFrame();

		Graph.AddPass(FRDGPassDescriptor{
			"PreProcess",
			{},
			{},
			[d, CommandContext]()
			{
				if (d->PreProcess)
					d->PreProcess->Draw(*CommandContext);
			}});

		const bool bScheduleShadowPass = !shadowCasters.empty() || ShadowProjectorSceneMoved.bValid;
		if (bScheduleShadowPass)
		{
			Graph.AddPass(FRDGPassDescriptor{
				"Shadow",
				{},
				{},
				[d, CommandContext, shadowCasters, shadowFrustumBounds, ShadowPassLights = std::move(ShadowPassLights), ShadowProjectorScene = std::move(ShadowProjectorSceneMoved)]() mutable
				{
					d->ShadowRender->Render(shadowCasters, shadowFrustumBounds, *CommandContext, std::move(ShadowPassLights), ShadowProjectorScene);
				}});
		}
		else if (d->ShadowRender)
		{
			d->ShadowRender->InvalidateCachedMainLightForShading();
		}

		const std::vector<FRDGPassResource> SceneTexturesIO = GatherSceneTexturesPassResources(TB);

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

				auto DepthTex = d->TargetBuffer->GetDepth();
				auto SceneCol = d->TargetBuffer->GetSceneColor();
				auto Motion = d->TargetBuffer->GetMotionVector();
				auto Emissive = d->TargetBuffer->GetEmissiveBuffer();
				auto Normal = d->TargetBuffer->GetNormalBuffer();
				auto MR = d->TargetBuffer->GetMetallicRoughnessBuffer();
				auto MatAux = d->TargetBuffer->GetMaterialAuxBuffer();
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
				CommandContext->SetRenderTarget(Targets, d->TargetBuffer->GetDepth());
			}});

		Graph.AddPass(FRDGPassDescriptor{
			"RenderSky",
			SceneTexturesIO,
			SceneTexturesIO,
			[d, CommandContext, ViewConst]()
			{
				std::vector<std::shared_ptr<RenderCore::RHITexture2D>> Targets = {
					d->TargetBuffer->GetSceneColor(), d->TargetBuffer->GetMotionVector(), d->TargetBuffer->GetNormalBuffer(),
					d->TargetBuffer->GetEmissiveBuffer(), d->TargetBuffer->GetMetallicRoughnessBuffer()};
				if (ViewConst && ViewConst->SkyLightIBLScale > 0.f && d->PreProcess)
				{
					auto IBL = d->PreProcess->GetIBLRender();
					auto SkyCube = IBL ? IBL->GetSkyLightCubemap() : nullptr;
					d->BackgroundRender->SetTextureCube(SkyCube);
					d->BackgroundRender->Render(*CommandContext, Targets, d->TargetBuffer->GetDepth());
				}
				else
				{
					d->BackgroundRender->SetTextureCube(nullptr);
				}
			}});

		Graph.AddPass(FRDGPassDescriptor{
			"RenderBasePass",
			SceneTexturesIO,
			SceneTexturesIO,
			[d, Self, RHI, CommandContext, MeshesForDraw, ViewConst]()
			{
				if (!MeshesForDraw->empty())
				{
					FDeferredBasePassDrawContext DrawContext;
					DrawContext.ViewData = ViewConst;
					DrawContext.TargetBuffer = d->TargetBuffer;
					DrawContext.WorldSceneRender = Self;
					DrawContext.RHICmdList = CommandContext.get();
					FDeferredShadingBasePassRenderer::RenderBasePassOpaque(RHI, *MeshesForDraw, DrawContext, *d->MeshMaterialRenderCache);
				}
			}});

		Graph.AddPass(FRDGPassDescriptor{
			"RenderTranslucency",
			SceneTexturesIO,
			SceneTexturesIO,
			[d, Self, RHI, CommandContext, MeshesForDraw, ViewConst]()
			{
				if (!MeshesForDraw->empty())
				{
					FDeferredBasePassDrawContext DrawContext;
					DrawContext.ViewData = ViewConst;
					DrawContext.TargetBuffer = d->TargetBuffer;
					DrawContext.WorldSceneRender = Self;
					DrawContext.RHICmdList = CommandContext.get();
					FDeferredShadingBasePassRenderer::RenderBasePassTranslucent(RHI, *MeshesForDraw, DrawContext, *d->MeshMaterialRenderCache);
				}
			}});

		if (d->DeferredLighting && TB && ViewConst && !ViewConst->bUnlit)
		{
			Graph.AddPass(FRDGPassDescriptor{
				FRDGDeferredLightingPass::PassNameCopySceneToPreLighting,
				FRDGDeferredLightingPass::GatherCopyPassInputs(TB),
				FRDGDeferredLightingPass::GatherCopyPassOutputs(TB),
				[d, CommandContext, TB]()
				{
					if (!d->DeferredLighting)
						return;
					d->DeferredLighting->CopySceneColorToPreLighting(*CommandContext, TB);
				},
				true,
				RDG_Copy,
				ERDGPassQueue::Graphics,
				true});
			Graph.AddPass(FRDGPassDescriptor{
				FRDGDeferredLightingPass::PassNameRaster,
				FRDGDeferredLightingPass::GatherRasterPassInputs(TB),
				FRDGDeferredLightingPass::GatherRasterPassOutputs(TB),
				[d, Self, CommandContext, ViewConst, TB]()
				{
					if (!d->DeferredLighting || !d->MainViewPort)
						return;
					d->DeferredLighting->ExecuteRaster(*CommandContext, d->MainViewPort, TB, Self, ViewConst);
				},
				true,
				RDG_Raster,
				ERDGPassQueue::Graphics,
				true});
		}

		d->PostProcess->AddFramePasses(Graph, *CommandContext, d->TargetBuffer, d->MainViewPort, ViewConst);

		Graph.AddPass(FRDGPassDescriptor{
			"ImGuiEncode",
			{},
			{},
			[d, Self]()
			{
				Self->sigGuiEvent();
				d->MainViewPort->RHIImGuiRenderDrawData();
			}});

		Graph.AddPass(FRDGPassDescriptor{
			"RHISubmitAndPresent",
			{},
			{},
			[d]()
			{
				d->MainViewPort->RHISubmitAndPresentFrame();
			}});

		if (bScheduleShadowPass)
		{
			Graph.AddPassDependency("Shadow", "ClearSceneTextures");
			Graph.AddPassDependency("Shadow", "RenderBasePass");
			Graph.AddPassDependency("Shadow", "RenderTranslucency");
			if (d->DeferredLighting && TB && ViewConst && !ViewConst->bUnlit)
				Graph.AddPassDependency("Shadow", FRDGDeferredLightingPass::PassNameRaster);
		}

		(void)ViewFamily;
		FRDGCompileParameters RDGExecParams = d->RDGCompileParams;
		RDGExecParams.RDGBarrierCommandContext = CommandContext.get();
		if (!Graph.Compile(d->RDGCompileParams, nullptr))
		{
			core::LOG(core::log_err, L"FRDG: frame graph compile failed (cycle); executing passes in AddPass order so Present still runs.");
			Graph.ExecutePassesInSetupOrder(RDGExecParams);
		}
		else
			Graph.ExecutePasses(RDGExecParams);
		RHI->RHIEndFrame();
	}

} // namespace Engine
