#include "Render/SceneRendering/FSceneRenderer.h"
#include "Render/SceneRender.h"
#include "Render/SceneRenderPrivate.h"
#include "Render/PreProcessor.h"
#include "Render/IBLRender.h"
#include "Render/PostProcessor.h"
#include "Render/CubeBackground.h"
#include "Render/GBuffer.h"
#include "Render/FrameGraph.h"
#include "Render/SceneRendering/FDeferredShadingBasePassRenderer.h"
#include "Render/SceneRendering/FMeshMaterialRenderCache.h"
#include "Render/SceneRendering/FDeferredBasePassDrawContext.h"
#include "Render/Shadow/ShadowRenderPass.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIViewPort.h"
#include "RHI/RHICommandContext.h"
#include "Scene/Actor.h"
#include "Engine.h"
#include "App/AppWindow.h"

using namespace RenderCore;

namespace Engine
{
	namespace
	{
		std::vector<FrameGraphResource> MakeGBufferResourcesForFrameGraph(const std::shared_ptr<GBuffer>& TB)
		{
			if (!TB)
				return {};
			return {
				{ "SceneColor", [TB]() { return TB->GetSceneColor(); } },
				{ "MotionVector", [TB]() { return TB->GetMotionVector(); } },
				{ "Normal", [TB]() { return TB->GetNormalBuffer(); } },
				{ "Emissive", [TB]() { return TB->GetEmissiveBuffer(); } },
				{ "MetallicRoughness", [TB]() { return TB->GetMetallicRoughnessBuffer(); } },
				{ "Depth", [TB]() { return TB->GetDepth(); } },
			};
		}
	} // namespace

	void FSceneRenderer::Submit(SceneRender* InSceneRenderSelf, SceneRenderPrivate* InResourceState, const FSceneViewFamily& InViewFamily,
								std::shared_ptr<const FSceneViewData> InViewData, std::vector<GltfSceneMeshInfo> MeshesInfoCopy,
								std::vector<GltfSceneMeshInfo> shadowCasters, std::vector<GltfSceneMeshInfo> shadowFrustumBounds,
								std::vector<Light> ShadowPassLights, std::shared_ptr<Actor> InShadowProjectorActor,
								std::vector<std::shared_ptr<Actor>> InAllActorsForShadow)
	{
		SceneRenderSelf = InSceneRenderSelf;
		ResourceState = InResourceState;
		ViewFamily = InViewFamily;
		ViewData = std::move(InViewData);
		MeshesInfo = std::move(MeshesInfoCopy);
		ShadowCasters = std::move(shadowCasters);
		ShadowFrustumBounds = std::move(shadowFrustumBounds);
		LightsForShadow = std::move(ShadowPassLights);
		ShadowProjectorActor = std::move(InShadowProjectorActor);
		AllActorsForShadow = std::move(InAllActorsForShadow);
		bHasFrame = (SceneRenderSelf != nullptr) && (ResourceState != nullptr) && (ViewData != nullptr);
	}

	void FSceneRenderer::Render(DynamicRHI* RHI)
	{
		if (!bHasFrame || !RHI || !ResourceState || !ViewData)
			return;

		bHasFrame = false;

		SceneRender* const Self = SceneRenderSelf;
		SceneRenderPrivate* const d = ResourceState;
		std::shared_ptr<const FSceneViewData> ViewConst = ViewData;
		std::vector<GltfSceneMeshInfo> MeshesInfoCopy = std::move(MeshesInfo);
		std::vector<GltfSceneMeshInfo> shadowCasters = std::move(ShadowCasters);
		std::vector<GltfSceneMeshInfo> shadowFrustumBounds = std::move(ShadowFrustumBounds);
		std::vector<Light> ShadowPassLights = std::move(LightsForShadow);
		std::shared_ptr<Actor> ShadowProjectorActorMoved = std::move(ShadowProjectorActor);
		std::vector<std::shared_ptr<Actor>> AllActorsForShadowMoved = std::move(AllActorsForShadow);

		ViewData.reset();
		SceneRenderSelf = nullptr;
		ResourceState = nullptr;

		std::shared_ptr<RHICommandContext> CommandContext = RHI->GetDefaultCommandContext();
		if (!CommandContext)
			return;

		auto MeshesForDraw = std::make_shared<std::vector<GltfSceneMeshInfo>>(std::move(MeshesInfoCopy));

		FrameGraph Graph;
		auto TB = d->TargetBuffer;
		RHI->RHIBeginFrame();

		Graph.AddPass(FramePassDesc{
			"PreProcess",
			{},
			{},
			[d, CommandContext]()
			{
				if (d->PreProcess)
					d->PreProcess->Draw(*CommandContext);
			}});

		if (!shadowCasters.empty())
		{
			Graph.AddPass(FramePassDesc{
				"Shadow",
				{},
				{},
				[d, CommandContext, shadowCasters, shadowFrustumBounds, ShadowPassLights = std::move(ShadowPassLights), ShadowProjectorActor = std::move(ShadowProjectorActorMoved),
				 AllActorsForShadow = std::move(AllActorsForShadowMoved)]() mutable
				{
					d->ShadowRender->Render(shadowCasters, shadowFrustumBounds, *CommandContext, ShadowPassLights, ShadowProjectorActor, AllActorsForShadow);
				}});
		}

		const std::vector<FrameGraphResource> GBufferIO = MakeGBufferResourcesForFrameGraph(TB);

		Graph.AddPass(FramePassDesc{
			"ClearGBuffer",
			{},
			GBufferIO,
			[d, RHI]()
			{
				d->MainViewPort->SetRenderTarget();
				d->MainViewPort->Clear(d->Color);
				d->MainViewPort->Prepare();
				int32_t width = GEngine->GetAppWindow()->GetWidth();
				int32_t height = GEngine->GetAppWindow()->GetHeight();
				RHI->GetDefaultCommandContext()->SetViewPort(0, 0, width, height);

				std::vector<std::shared_ptr<RenderCore::RHITexture2D>> Targets = {
					d->TargetBuffer->GetSceneColor(), d->TargetBuffer->GetMotionVector(), d->TargetBuffer->GetNormalBuffer(),
					d->TargetBuffer->GetEmissiveBuffer(), d->TargetBuffer->GetMetallicRoughnessBuffer()};
				RHI->GetDefaultCommandContext()->Clear(Targets, d->TargetBuffer->GetDepth(), core::FLinearColor::Black, 1.f, 0);
			}});

		Graph.AddPass(FramePassDesc{
			"RenderSky",
			GBufferIO,
			GBufferIO,
			[d, RHI]()
			{
				std::vector<std::shared_ptr<RenderCore::RHITexture2D>> Targets = {
					d->TargetBuffer->GetSceneColor(), d->TargetBuffer->GetMotionVector(), d->TargetBuffer->GetNormalBuffer(),
					d->TargetBuffer->GetEmissiveBuffer(), d->TargetBuffer->GetMetallicRoughnessBuffer()};
				auto IBL = d->PreProcess->GetIBLRender();
				auto SkyCube = IBL->GetSkyLightCubemap();
				d->BackgroundRender->SetTextureCube(SkyCube);
				d->BackgroundRender->Render(*RHI->GetDefaultCommandContext(), Targets, d->TargetBuffer->GetDepth());
			}});

		Graph.AddPass(FramePassDesc{
			"RenderBasePass",
			GBufferIO,
			GBufferIO,
			[d, Self, RHI, MeshesForDraw, ViewConst]()
			{
				if (!MeshesForDraw->empty())
				{
					FDeferredBasePassDrawContext DrawContext;
					DrawContext.ViewData = ViewConst;
					DrawContext.TargetBuffer = d->TargetBuffer;
					DrawContext.SceneRenderRaw = Self;
					FDeferredShadingBasePassRenderer::RenderBasePassOpaque(RHI, *MeshesForDraw, DrawContext, *d->MeshMaterialRenderCache);
				}
			}});

		Graph.AddPass(FramePassDesc{
			"RenderTranslucency",
			GBufferIO,
			GBufferIO,
			[d, Self, RHI, MeshesForDraw, ViewConst]()
			{
				if (!MeshesForDraw->empty())
				{
					FDeferredBasePassDrawContext DrawContext;
					DrawContext.ViewData = ViewConst;
					DrawContext.TargetBuffer = d->TargetBuffer;
					DrawContext.SceneRenderRaw = Self;
					FDeferredShadingBasePassRenderer::RenderBasePassTranslucent(RHI, *MeshesForDraw, DrawContext, *d->MeshMaterialRenderCache);
				}
			}});

		d->PostProcess->AddFramePasses(Graph, *RHI->GetDefaultCommandContext(), d->TargetBuffer, d->MainViewPort, ViewConst);

		Graph.AddPass(FramePassDesc{
			"ImGuiEncode",
			{},
			{},
			[d, Self]()
			{
				Self->sigGuiEvent();
				d->MainViewPort->RHIImGuiRenderDrawData();
			}});

		Graph.AddPass(FramePassDesc{
			"RHISubmitAndPresent",
			{},
			{},
			[d]()
			{
				d->MainViewPort->RHISubmitAndPresentFrame();
			}});

		(void)ViewFamily;
		Graph.Execute(d->RDGCompileParams);
		RHI->RHIEndFrame();
	}

} // namespace Engine
