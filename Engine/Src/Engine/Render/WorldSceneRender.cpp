#include "Render/WorldSceneRender.h"
#include "Render/WorldSceneRenderPrivate.h"
#include "Scene/World.h"
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
#include "Render/IBLRender.h"
#include "Render/PostProcessor.h"
#include "Render/RDGBuilder.h"
#include "Render/GBuffer.h"
#include "Render/RenderTexturePool.h"
#include "Render/Shadow/ShadowRenderPass.h"
#include "Render/Shadow/ShadowMap.h"
#include "Scene/Component.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "Render/SceneRendering/SceneViewFamily.h"
#include "Render/SceneRendering/SceneRenderer.h"
#include "Render/SceneRendering/DeferredLightingPass.h"
#include "core/logger.h"
#include <exception>
#include <mutex>
#include <optional>
#include <string>

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
			}
			catch (const std::exception&)
			{
			}
		}
	} // namespace

	FWorldSceneRender::FWorldSceneRender(std::weak_ptr<World> Owner)
		: d_ptr(new FWorldSceneRenderPrivate())
	{
		C_P(FWorldSceneRender);
		d->Owner = std::move(Owner);
		d->MeshMaterialRenderCache = std::make_unique<FMeshMaterialRenderCache>();
	}

	FWorldSceneRender::~FWorldSceneRender()
	{
		if (GRenderThread)
		{
			GRenderThread->WaitForFinish();
		}
		delete d_ptr;
	}

	std::shared_ptr<World> FWorldSceneRender::GetWorld() const
	{
		C_P(FWorldSceneRender);
		return d->Owner.lock();
	}

	void FWorldSceneRender::InitResource(std::shared_ptr<RHIViewPort> ViewPort)
	{
		C_P(FWorldSceneRender);
		d->MainViewPort = ViewPort;

		ENQUEUE_UNIQUE_RENDER_COMMAND([d](RenderCore::DynamicRHI* RHI) {
			if (!d->PreProcess)
				d->PreProcess = std::make_shared<PreProcessor>(RHI);
			d->PreProcess->InitResource();

			if (!d->PostProcess)
				d->PostProcess = std::make_shared<PostProcessor>(RHI);
			d->PostProcess->InitResource();

			if (!d->BackgroundRender)
				d->BackgroundRender = std::make_shared<CubeBackground>(RHI);
			d->BackgroundRender->InitResource();

			if (!d->TargetBuffer)
				d->TargetBuffer = std::make_shared<GBuffer>(RHI);
			auto Size = d->MainViewPort->GetSize();
			d->TargetBuffer->InitDefaultSceneTargets(Size.cx, Size.cy);

			if (!d->ShadowRender)
				d->ShadowRender = std::make_shared<ShadowRenderPass>(RHI);
			d->ShadowRender->InitResource();

			if (!d->DeferredLighting)
				d->DeferredLighting = std::make_shared<DeferredLightingPass>(RHI);
			d->DeferredLighting->InitResource();
			d->IsInit = true;
		});
	}

	void FWorldSceneRender::LoadConfig(const nlohmann::json& Root)
	{
		C_P(FWorldSceneRender);
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
		ENQUEUE_UNIQUE_RENDER_COMMAND([d, Root](RenderCore::DynamicRHI* RHI) {
			if (d->PreProcess)
				d->PreProcess->LoadConfig(Root);
			if (d->PostProcess)
				d->PostProcess->LoadConfig(Root);
		});
	}

	void FWorldSceneRender::SetBackgroundColor(const core::FLinearColor& Color)
	{
		C_P(FWorldSceneRender);
		d->Color = Color;
	}

	void FWorldSceneRender::Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen)
	{
		if (InSizeX == 0 || InSizeY == 0)
		{
			return;
		}
		C_P(FWorldSceneRender);
		ENQUEUE_UNIQUE_RENDER_COMMAND([d, InSizeX, InSizeY, bInIsFullscreen](RenderCore::DynamicRHI* RHI) {
			d->MainViewPort->Resize(InSizeX, InSizeY, bInIsFullscreen);
			if (d->TargetBuffer)
				d->TargetBuffer->InitDefaultSceneTargets(InSizeX, InSizeY);
			if (d->PostProcess)
				d->PostProcess->InvalidateTransientResources();
		});
	}

	void FWorldSceneRender::Render(float DeltaTime)
	{
		C_P(FWorldSceneRender);
		SubmitSceneForRendering(DeltaTime);
	}

	void FWorldSceneRender::SetIBLRotate(float x, float y)
	{
		C_P(FWorldSceneRender);
		d->BackgroundRender->SetRotate(x, y);
		d->DeferredBasePassEnvironmentRotateX = x;
		d->DeferredBasePassEnvironmentRotateY = y;
	}

	std::shared_ptr<PreProcessor> FWorldSceneRender::GetPreProcessor() const
	{
		C_P(const FWorldSceneRender);
		return d->PreProcess;
	}

	std::shared_ptr<PostProcessor> FWorldSceneRender::GetPostProcessor() const
	{
		C_P(const FWorldSceneRender);
		return d->PostProcess;
	}

	std::shared_ptr<ShadowRenderPass> FWorldSceneRender::GetShadowRenderPass() const
	{
		C_P(const FWorldSceneRender);
		return d->ShadowRender;
	}

	std::shared_ptr<RHIViewPort> FWorldSceneRender::GetViewPort() const
	{
		C_P(const FWorldSceneRender);
		return d->MainViewPort;
	}

	void FWorldSceneRender::SubmitSceneForRendering(float DeltaTime)
	{
		(void)DeltaTime;
		C_P(FWorldSceneRender);
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
		Primary.BuildFromCamera(*World->GetMainCamera(), std::move(lightsSnapshot), d->DeferredBasePassEnvironmentRotateX, d->DeferredBasePassEnvironmentRotateY,
								bHaltonProjJitter, 0, 0, (int32_t)ViewFamily.RenderSizeX, (int32_t)ViewFamily.RenderSizeY);
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

		std::lock_guard<std::mutex> FrameLock(d->RenderFrameMutex);
		d->SceneRenderer.Submit(this, d, ViewFamily, ViewConst, std::move(MeshesInfoCopy), std::move(shadowCasters), std::move(shadowFrustumBounds),
									 std::move(shadowLights), shadowProjectorScene, std::move(skyLightHdrOverride));

		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[d](DynamicRHI* RHIIn)
			{
				d->SceneRenderer.Render(RHIIn);
			},
			true);
	}

} // namespace Engine
