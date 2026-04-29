#include "Render/SceneRender.h"
#include "Render/SceneRenderPrivate.h"
#include "Scene/World.h"
#include "RHI/RHICommandContext.h"
#include "Scene/Actor.h"
#include "Scene/CameraComponent.h"
#include "Scene/GltfMeshComponent.h"
#include "RHI/RHIViewPort.h"
#include "Thread/RenderThread.h"
#include "Engine.h"
#include "RHI/DynamicRHI.h"
#include "App/AppWindow.h"
#include "Render/PreProcessor.h"
#include "GltfModel/GltfMesh.h"
#include "Render/SceneRendering/FMeshMaterialRenderCache.h"
#include "Render/SceneRendering/FSceneRendererPrimitiveGather.h"
#include "Render/CubeBackground.h"
#include "Render/IBLRender.h"
#include "Render/PostProcessor.h"
#include "Render/FrameGraph.h"
#include "Render/GBuffer.h"
#include "Render/RenderTexturePool.h"
#include "Render/Shadow/ShadowRenderPass.h"
#include "Render/SceneRendering/FSceneViewData.h"
#include "Render/SceneRendering/FSceneViewFamily.h"
#include "Render/SceneRendering/FSceneRenderer.h"
#include "core/logger.h"
#include <optional>

using namespace RenderCore;

namespace Engine
{
	namespace
	{
		void ApplyRDGCompileParamsFromJson(const nlohmann::json& Root, FrameGraphCompileParams& Out)
		{
			try
			{
				if (Root.find("RDG") == Root.end() || !Root["RDG"].is_object())
					return;
				const auto& J = Root["RDG"];
				Out.bPassCullingFromSinks = J.value("PassCullingFromSinks", Out.bPassCullingFromSinks);
				Out.bDumpDotToLog = J.value("DumpDotToLog", Out.bDumpDotToLog);
				Out.bLogCompileSummary = J.value("LogCompileSummary", Out.bLogCompileSummary);
				Out.bLogRenderTexturePoolStats = J.value("LogRenderTexturePoolStats", Out.bLogRenderTexturePoolStats);
			}
			catch (const std::exception&)
			{
			}
		}
	} // namespace

	SceneRender::SceneRender(std::weak_ptr<World> Owner)
		: d_ptr(new SceneRenderPrivate())
	{
		C_P(SceneRender);
		d->Owner = std::move(Owner);
		d->MeshMaterialRenderCache = std::make_unique<FMeshMaterialRenderCache>();
	}

	SceneRender::~SceneRender()
	{
		if (GRenderThread)
		{
			GRenderThread->WaitForFinish();
		}
		delete d_ptr;
	}

	std::shared_ptr<World> SceneRender::GetWorld() const
	{
		C_P(SceneRender);
		return d->Owner.lock();
	}

	void SceneRender::InitResource(std::shared_ptr<RHIViewPort> ViewPort)
	{
		C_P(SceneRender);
		d->MainViewPort = ViewPort;

		ENQUEUE_UNIQUE_RENDER_COMMAND([d](RenderCore::DynamicRHI* RHI) {
			if (!d->PreProcess)
			{
				d->PreProcess = std::make_shared<PreProcessor>(RHI);
			}
			d->PreProcess->InitResource();
			if (!d->PostProcess)
			{
				d->PostProcess = std::make_shared<PostProcessor>(RHI);
			}
			d->PostProcess->InitResource();

			if (!d->BackgroundRender)
			{
				d->BackgroundRender = std::make_shared<CubeBackground>(RHI);
			}
			d->BackgroundRender->InitResource();

			if (!d->TargetBuffer)
			{
				d->TargetBuffer = std::make_shared<GBuffer>(RHI);
			}
			auto Size = d->MainViewPort->GetSize();
			d->TargetBuffer->InitDefaultSceneTargets(Size.cx, Size.cy);

			if (!d->ShadowRender)
			{
				d->ShadowRender = std::make_shared<ShadowRenderPass>(RHI);
			}
			d->ShadowRender->InitResource();
			d->IsInit = true;
		});
	}

	void SceneRender::LoadConfig(const nlohmann::json& Root)
	{
		C_P(SceneRender);
		ApplyRDGCompileParamsFromJson(Root, d->RDGCompileParams);
		RenderTexturePool::Get().ApplyConfigFromJson(Root);
		ENQUEUE_UNIQUE_RENDER_COMMAND([d, Root](RenderCore::DynamicRHI* RHI) {
			if (d->PreProcess)
				d->PreProcess->LoadConfig(Root);
			if (d->PostProcess)
				d->PostProcess->LoadConfig(Root);
		});
	}

	void SceneRender::SetBackgroundColor(const core::FLinearColor& Color)
	{
		C_P(SceneRender);
		d->Color = Color;
	}

	void SceneRender::Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen)
	{
		if (InSizeX == 0 || InSizeY == 0)
		{
			return;
		}
		C_P(SceneRender);
		ENQUEUE_UNIQUE_RENDER_COMMAND([d, InSizeX, InSizeY, bInIsFullscreen](RenderCore::DynamicRHI* RHI) {
			d->MainViewPort->Resize(InSizeX, InSizeY, bInIsFullscreen);
			if (d->TargetBuffer)
				d->TargetBuffer->InitDefaultSceneTargets(InSizeX, InSizeY);
			if (d->PostProcess)
				d->PostProcess->InvalidateTransientResources();
		});
	}

	void SceneRender::Render(float DeltaTime)
	{
		C_P(SceneRender);
		RenderScene(DeltaTime);
	}

	void SceneRender::SetIBLRotate(float x, float y)
	{
		C_P(SceneRender);
		d->BackgroundRender->SetRotate(x, y);
		d->DeferredBasePassEnvironmentRotateX = x;
		d->DeferredBasePassEnvironmentRotateY = y;
	}

	std::shared_ptr<PreProcessor> SceneRender::GetPreProcessor() const
	{
		C_P(const SceneRender);
		return d->PreProcess;
	}

	std::shared_ptr<PostProcessor> SceneRender::GetPostProcessor() const
	{
		C_P(const SceneRender);
		return d->PostProcess;
	}

	bool SceneRender::UsesTemporalAAProjectionJitter() const
	{
		C_P(const SceneRender);
		if (!d->PostProcess)
			return false;
		return d->PostProcess->GetPostProcessorAAType() == EPostProcessorAAType::TAA;
	}

	std::shared_ptr<ShadowRenderPass> SceneRender::GetShadowRenderPass() const
	{
		C_P(const SceneRender);
		return d->ShadowRender;
	}

	std::shared_ptr<RHIViewPort> SceneRender::GetViewPort() const
	{
		C_P(const SceneRender);
		return d->MainViewPort;
	}

	void SceneRender::RenderScene(float DeltaTime)
	{
		(void)DeltaTime;
		C_P(SceneRender);
		std::shared_ptr<World> World = GetWorld();
		if (!d->IsInit || !World || !World->GetMainCamera())
			return;

		FSceneViewFamily ViewFamily;
		ViewFamily.RenderSizeX = (uint32_t)d->MainViewPort->GetSize().cx;
		ViewFamily.RenderSizeY = (uint32_t)d->MainViewPort->GetSize().cy;
		ViewFamily.bUsesTemporalAAProjectionJitter = UsesTemporalAAProjectionJitter();
		ViewFamily.Views.resize(1);
		FSceneViewData& Primary = ViewFamily.PrimaryView();
		std::vector<Light> lightsSnapshot(World->GetLights().begin(), World->GetLights().end());
		Primary.BuildFromCamera(*World->GetMainCamera(), std::move(lightsSnapshot), d->DeferredBasePassEnvironmentRotateX, d->DeferredBasePassEnvironmentRotateY,
								ViewFamily.bUsesTemporalAAProjectionJitter, 0, 0, (int32_t)ViewFamily.RenderSizeX, (int32_t)ViewFamily.RenderSizeY);

		auto ViewDataPtr = std::make_shared<FSceneViewData>(Primary);
		std::shared_ptr<const FSceneViewData> ViewConst = ViewDataPtr;

		std::vector<std::shared_ptr<Actor>> actorsCopy;
		{
			const auto& liveActors = World->GetAllActors();
			actorsCopy.assign(liveActors.begin(), liveActors.end());
		}

		FPrimitiveGatherResult PrimitiveGather;
		FSceneRendererPrimitiveGather::GatherVisiblePrimitives(*ViewConst, actorsCopy, PrimitiveGather);

		std::vector<GltfSceneMeshInfo> MeshesInfoCopy = std::move(PrimitiveGather.VisiblePrimitives);
		std::vector<GltfSceneMeshInfo> shadowCasters = std::move(PrimitiveGather.DynamicShadowCastingPrimitives);
		std::vector<GltfSceneMeshInfo> shadowFrustumBounds = std::move(PrimitiveGather.ShadowFrustumCullPrimitives);

		std::shared_ptr<Actor> shadowProjector = World->GetShadowProjectorActor();

		std::vector<Light> shadowLights(ViewConst->Lights.begin(), ViewConst->Lights.end());

		auto RHI = GEngine->GetRHI();
		if (!RHI)
			return;

		d->SceneFrameRenderer.Submit(this, d, ViewFamily, ViewConst, std::move(MeshesInfoCopy), std::move(shadowCasters), std::move(shadowFrustumBounds),
									 std::move(shadowLights), std::move(shadowProjector), std::move(actorsCopy));

		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[d](DynamicRHI* RHIIn)
			{
				d->SceneFrameRenderer.Render(RHIIn);
			},
			true);
	}

} // namespace Engine
