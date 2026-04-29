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
#include "Engine/Thread/RenderThread.h"
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

	void FSceneRenderer::ExecuteDeferredFrame(DynamicRHI* RHI, SceneRender* SceneRenderSelf, SceneRenderPrivate* d, const FSceneViewFamily& ViewFamily,
											std::shared_ptr<const FSceneViewData> ViewData, std::vector<GltfSceneMeshInfo> MeshesInfoCopy, std::vector<GltfSceneMeshInfo> shadowCasters,
											std::vector<GltfSceneMeshInfo> shadowFrustumBounds, std::vector<Light> ShadowPassLights, std::shared_ptr<Actor> ShadowProjectorActor,
											std::vector<std::shared_ptr<Actor>> AllActorsForShadow)
	{
		(void)ViewFamily;
		if (!RHI || !d || !ViewData)
			return;

		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[d, SceneRenderSelf, ViewData, shadowCasters = std::move(shadowCasters), shadowFrustumBounds = std::move(shadowFrustumBounds),
			 MeshesInfo = std::move(MeshesInfoCopy), LightsForShadow = std::move(ShadowPassLights), ShadowProjectorActor = std::move(ShadowProjectorActor),
			 AllActorsForShadow = std::move(AllActorsForShadow)](DynamicRHI* RHIIn)
			{
				std::shared_ptr<RHICommandContext> CommandContext = RHIIn->GetDefaultCommandContext();
				if (!CommandContext)
					return;

				FrameGraph Graph;
				auto TB = d->TargetBuffer;
				RHIIn->RHIBeginFrame();

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
						[d, CommandContext, shadowCasters, shadowFrustumBounds, LightsForShadow, ShadowProjectorActor, AllActorsForShadow]()
						{
							d->ShadowRender->Render(shadowCasters, shadowFrustumBounds, *CommandContext, LightsForShadow, ShadowProjectorActor, AllActorsForShadow);
						}});
				}

				const std::vector<FrameGraphResource> GBufferIO = MakeGBufferResourcesForFrameGraph(TB);

				Graph.AddPass(FramePassDesc{
					"ClearGBuffer",
					{},
					GBufferIO,
					[d, RHIIn]()
					{
						d->MainViewPort->SetRenderTarget();
						d->MainViewPort->Clear(d->Color);
						d->MainViewPort->Prepare();
						int32_t width = GEngine->GetAppWindow()->GetWidth();
						int32_t height = GEngine->GetAppWindow()->GetHeight();
						RHIIn->GetDefaultCommandContext()->SetViewPort(0, 0, width, height);

						std::vector<std::shared_ptr<RenderCore::RHITexture2D>> Targets = {
							d->TargetBuffer->GetSceneColor(), d->TargetBuffer->GetMotionVector(), d->TargetBuffer->GetNormalBuffer(),
							d->TargetBuffer->GetEmissiveBuffer(), d->TargetBuffer->GetMetallicRoughnessBuffer()};
						RHIIn->GetDefaultCommandContext()->Clear(Targets, d->TargetBuffer->GetDepth(), core::FLinearColor::Black, 1.f, 0);
					}});

				Graph.AddPass(FramePassDesc{
					"RenderSky",
					GBufferIO,
					GBufferIO,
					[d, RHIIn]()
					{
						std::vector<std::shared_ptr<RenderCore::RHITexture2D>> Targets = {
							d->TargetBuffer->GetSceneColor(), d->TargetBuffer->GetMotionVector(), d->TargetBuffer->GetNormalBuffer(),
							d->TargetBuffer->GetEmissiveBuffer(), d->TargetBuffer->GetMetallicRoughnessBuffer()};
						auto IBL = d->PreProcess->GetIBLRender();
						auto SkyCube = IBL->GetSkyLightCubemap();
						d->BackgroundRender->SetTextureCube(SkyCube);
						d->BackgroundRender->Render(*RHIIn->GetDefaultCommandContext(), Targets, d->TargetBuffer->GetDepth());
					}});

				Graph.AddPass(FramePassDesc{
					"RenderBasePass",
					GBufferIO,
					GBufferIO,
					[d, SceneRenderSelf, RHIIn, MeshesInfo, ViewData]()
					{
						if (!MeshesInfo.empty())
						{
							FDeferredBasePassDrawContext DrawContext;
							DrawContext.ViewData = ViewData;
							DrawContext.TargetBuffer = d->TargetBuffer;
							DrawContext.SceneRenderRaw = SceneRenderSelf;
							FDeferredShadingBasePassRenderer::RenderBasePassOpaque(RHIIn, MeshesInfo, DrawContext, *d->MeshMaterialRenderCache);
						}
					}});

				Graph.AddPass(FramePassDesc{
					"RenderTranslucency",
					GBufferIO,
					GBufferIO,
					[d, SceneRenderSelf, RHIIn, MeshesInfo, ViewData]()
					{
						if (!MeshesInfo.empty())
						{
							FDeferredBasePassDrawContext DrawContext;
							DrawContext.ViewData = ViewData;
							DrawContext.TargetBuffer = d->TargetBuffer;
							DrawContext.SceneRenderRaw = SceneRenderSelf;
							FDeferredShadingBasePassRenderer::RenderBasePassTranslucent(RHIIn, MeshesInfo, DrawContext, *d->MeshMaterialRenderCache);
						}
					}});

				d->PostProcess->AddFramePasses(Graph, *RHIIn->GetDefaultCommandContext(), d->TargetBuffer, d->MainViewPort, ViewData);

				Graph.AddPass(FramePassDesc{
					"ImGuiEncode",
					{},
					{},
					[d, SceneRenderSelf]()
					{
						SceneRenderSelf->sigGuiEvent();
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

				Graph.Execute(d->RDGCompileParams);
				RHIIn->RHIEndFrame();
			},
			true);
	}

} // namespace Engine
