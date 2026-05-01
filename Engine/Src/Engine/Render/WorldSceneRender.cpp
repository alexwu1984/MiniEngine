#include "Render/WorldSceneRender.h"
#include "Render/WorldSceneRenderPrivate.h"
#include "Scene/World.h"
#include "Scene/SkyLightComponent.h"
#include "RHI/RHICommandContext.h"
#include "Scene/Actor.h"
#include "Scene/CameraComponent.h"
#include "RHI/RHIViewPort.h"
#include "Thread/RenderThread.h"
#include "Engine.h"
#include "RHI/DynamicRHI.h"
#include "App/AppWindow.h"
#include "Render/PreProcessor.h"
#include "GltfModel/GltfMesh.h"
#include "Render/SceneRendering/MeshMaterialRenderCache.h"
#include "Render/SceneRendering/SceneRendererPrimitiveGather.h"
#include "Render/CubeBackground.h"
#include "Render/SkyLightEnvironment.h"
#include "Render/PostProcessor.h"
#include "Render/RDGBuilder.h"
#include "Render/SceneTextures.h"
#include "Render/RenderTexturePool.h"
#include "Render/Shadow/ShadowRenderPass.h"
#include "Render/Shadow/ShadowMap.h"
#include "Scene/Component.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "Render/SceneRendering/SceneViewFamily.h"
#include "Render/SceneRendering/SceneRenderer.h"
#include "Render/SceneRendering/DeferredLightingPass.h"
#include "core/logger.h"

using namespace RenderCore;

namespace Engine
{
	namespace
	{
		void ApplyRDGCompileParamsFromJson(const nlohmann::json& Root, FRDGCompileParameters& Out)
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
				Out.bRDGAutoPipelineBarriers = J.value("AutoPipelineBarriers", Out.bRDGAutoPipelineBarriers);
			}
			catch (const std::exception&)
			{
			}
		}
	} // namespace

	FWorldSceneRender::FWorldSceneRender(std::weak_ptr<World> Owner)
		: d_ptr(std::make_shared<FWorldSceneRenderPrivate>())
	{
		FWorldSceneRenderPrivate* d = d_ptr.get();
		d->Owner = std::move(Owner);
		d->MeshMaterialRenderCache = std::make_unique<FMeshMaterialRenderCache>();
	}

	FWorldSceneRender::~FWorldSceneRender()
	{
		if (GRenderThread)
		{
			if (std::this_thread::get_id() != GRenderThread->GetWorkerThreadId())
				GRenderThread->WaitForFinish();
		}
	}

	std::shared_ptr<World> FWorldSceneRender::GetWorld() const
	{
		const FWorldSceneRenderPrivate* d = d_ptr.get();
		return d->Owner.lock();
	}

	void FWorldSceneRender::InitResource(std::shared_ptr<RHIViewPort> ViewPort)
	{
		FWorldSceneRenderPrivate* d = d_ptr.get();
		d->MainViewPort = ViewPort;

		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[dLife = d_ptr](RenderCore::DynamicRHI* RHI)
			{
				if (!dLife->PreProcess)
					dLife->PreProcess = std::make_shared<PreProcessor>(RHI);
				dLife->PreProcess->InitResource();

				if (!dLife->PostProcess)
					dLife->PostProcess = std::make_shared<PostProcessor>(RHI);
				dLife->PostProcess->InitResource();

				if (!dLife->BackgroundRender)
					dLife->BackgroundRender = std::make_shared<CubeBackground>(RHI);
				dLife->BackgroundRender->InitResource();

				if (!dLife->TargetBuffer)
					dLife->TargetBuffer = std::make_shared<SceneTextures>(RHI);
				auto Size = dLife->MainViewPort->GetSize();
				dLife->TargetBuffer->InitDefaultSceneTargets(Size.cx, Size.cy);

				if (!dLife->ShadowRender)
					dLife->ShadowRender = std::make_shared<ShadowRenderPass>(RHI);
				dLife->ShadowRender->InitResource();

				if (!dLife->DeferredLighting)
					dLife->DeferredLighting = std::make_shared<DeferredLightingPass>(RHI);
				dLife->DeferredLighting->InitResource();
				dLife->IsInit = true;
			});
	}

	void FWorldSceneRender::LoadConfig(const nlohmann::json& Root)
	{
		FWorldSceneRenderPrivate* d = d_ptr.get();
		ApplyRDGCompileParamsFromJson(Root, d->RDGCompileParams);
		RenderTexturePool::Get().ApplyConfigFromJson(Root);
		try
		{
			const auto EvnIt = Root.find("Evn");
			if (EvnIt != Root.end() && EvnIt->is_object())
			{
				const auto& Evn = *EvnIt;
				d->bUnlit = Evn.value("Unlit", Evn.value("ForceUnlit", Evn.value("UnlitView", false)));
			}
		}
		catch (const std::exception&)
		{
		}
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[dLife = d_ptr, Root](RenderCore::DynamicRHI* RHI)
			{
				if (dLife->PreProcess)
					dLife->PreProcess->LoadConfig(Root);
				if (dLife->PostProcess)
					dLife->PostProcess->LoadConfig(Root);
			});
	}

	void FWorldSceneRender::SetBackgroundColor(const core::FLinearColor& Color)
	{
		FWorldSceneRenderPrivate* d = d_ptr.get();
		d->Color = Color;
	}

	void FWorldSceneRender::Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen)
	{
		if (InSizeX == 0 || InSizeY == 0)
		{
			return;
		}
		FWorldSceneRenderPrivate* d = d_ptr.get();
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[dLife = d_ptr, InSizeX, InSizeY, bInIsFullscreen](RenderCore::DynamicRHI* RHI)
			{
				dLife->MainViewPort->Resize(InSizeX, InSizeY, bInIsFullscreen);
				if (dLife->TargetBuffer)
					dLife->TargetBuffer->InitDefaultSceneTargets(InSizeX, InSizeY);
				if (dLife->PostProcess)
					dLife->PostProcess->InvalidateTransientResources();
			});
	}

	void FWorldSceneRender::Render(float DeltaTime)
	{
		(void)DeltaTime;
		SubmitSceneForRendering(DeltaTime);
	}

	void FWorldSceneRender::SetIBLRotate(float x, float y)
	{
		FWorldSceneRenderPrivate* d = d_ptr.get();
		const std::shared_ptr<World> w = GetWorld();
		if (!w)
			return;
		const std::shared_ptr<SkyLightComponent> sl = w->FindPrimarySkyLightComponent();
		if (!sl)
			return;
		sl->SetIBLRotationPitchDegrees(x);
		sl->SetIBLRotationYawDegrees(y);
	}

	std::shared_ptr<PreProcessor> FWorldSceneRender::GetPreProcessor() const
	{
		const FWorldSceneRenderPrivate* d = d_ptr.get();
		return d->PreProcess;
	}

	std::shared_ptr<PostProcessor> FWorldSceneRender::GetPostProcessor() const
	{
		const FWorldSceneRenderPrivate* d = d_ptr.get();
		return d->PostProcess;
	}

	std::shared_ptr<ShadowRenderPass> FWorldSceneRender::GetShadowRenderPass() const
	{
		const FWorldSceneRenderPrivate* d = d_ptr.get();
		return d->ShadowRender;
	}

	std::shared_ptr<RHIViewPort> FWorldSceneRender::GetViewPort() const
	{
		const FWorldSceneRenderPrivate* d = d_ptr.get();
		return d->MainViewPort;
	}

	void FWorldSceneRender::RequestMeshMaterialRenderCacheInvalidate()
	{
		// Game thread sets flag; render thread clears FMeshMaterialRenderCache on next Render.
		FWorldSceneRenderPrivate* d = d_ptr.get();
		if (d)
			d->bMeshMaterialCacheInvalidatePending.store(true, std::memory_order_release);
	}

	void FWorldSceneRender::SubmitSceneForRendering(float DeltaTime)
	{
		(void)DeltaTime;
		FWorldSceneRenderPrivate* d = d_ptr.get();
		std::shared_ptr<World> World = GetWorld();
		if (!d->IsInit || !World || !World->GetMainCamera())
			return;

		FSceneViewFamily ViewFamily;
		ViewFamily.RenderSizeX = (uint32_t)d->MainViewPort->GetSize().cx;
		ViewFamily.RenderSizeY = (uint32_t)d->MainViewPort->GetSize().cy;
		const bool bHaltonProjJitter = d->PostProcess && d->PostProcess->WantsHaltonProjectionJitterForMainPass();
		ViewFamily.bHaltonProjectionJitterForMainPass = bHaltonProjJitter;
		ViewFamily.Views.resize(1);
		FSceneViewData& Primary = ViewFamily.PrimaryView();
		std::vector<Light> lightsSnapshot = World->GatherLightsForView();
		float envPitchDeg = 0.f, envYawDeg = 0.f;
		World->GetPrimarySkyLightIBLRotationDegrees(envPitchDeg, envYawDeg);
		Primary.BuildFromCamera(*World->GetMainCamera(), std::move(lightsSnapshot), envPitchDeg, envYawDeg, bHaltonProjJitter, 0, 0,
								(int32_t)ViewFamily.RenderSizeX, (int32_t)ViewFamily.RenderSizeY);
		Primary.bUnlit = d->bUnlit;
		Primary.SkyLightIBLScale = World->GetSkyLightIBLScale();

		auto ViewDataPtr = std::make_shared<FSceneViewData>(Primary);
		std::shared_ptr<const FSceneViewData> ViewConst = ViewDataPtr;

		std::vector<std::shared_ptr<Actor>> actorsCopy = World->GetAllActorsCopy();

		FPrimitiveGatherResult PrimitiveGather;
		FSceneRendererPrimitiveGather::GatherVisiblePrimitives(*ViewConst, actorsCopy, PrimitiveGather);

		std::vector<GltfSceneMeshInfo> MeshesInfoCopy = std::move(PrimitiveGather.VisiblePrimitives);
		std::vector<GltfSceneMeshInfo> shadowCasters = std::move(PrimitiveGather.DynamicShadowCastingPrimitives);
		std::vector<GltfSceneMeshInfo> shadowFrustumBounds = std::move(PrimitiveGather.ShadowFrustumCullPrimitives);

		const FShadowProjectorSceneData shadowProjectorScene = World->BuildShadowProjectorAggregateData();

		std::vector<Light> shadowLights(ViewConst->Lights.begin(), ViewConst->Lights.end());

		auto RHI = GEngine->GetRHI();
		if (!RHI)
			return;

		std::optional<std::wstring> skyLightHdrOverride = World->ResolvePrimarySkyLightHDRFullPath();

		{
			std::lock_guard<std::mutex> FrameLock(d->RenderFrameMutex);
			d->SceneRenderer.Submit(this, d, ViewFamily, ViewConst, std::move(MeshesInfoCopy), std::move(shadowCasters), std::move(shadowFrustumBounds),
											std::move(shadowLights), shadowProjectorScene, std::move(skyLightHdrOverride));
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[dLife = d_ptr](DynamicRHI* RHIIn)
			{
				dLife->SceneRenderer.Render(RHIIn);
			},
			true);
	}

} // namespace Engine
