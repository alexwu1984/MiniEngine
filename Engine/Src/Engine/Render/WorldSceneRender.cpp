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
#include "GltfModel/GltfMesh.h"
#include "Render/SceneRendering/SceneRendererPrimitiveGather.h"
#include "Render/SkyLightRenderPass.h"
#include "Render/SkyLightEnvironment.h"
#include "Render/PostProcessor.h"
#include "Render/RDGBuilder.h"
#include "Render/SceneTextures.h"
#include "Render/RenderTexturePool.h"
#include "Render/Shadow/ShadowRenderPass.h"
#include "Render/Shadow/ShadowMap.h"
#include "Scene/Component.h"
#include "Scene/FScene.h"
#include "Render/SceneRendering/MeshMaterialRenderCache.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "Render/SceneRendering/SceneViewFamily.h"
#include "Render/SceneRendering/SceneRenderer.h"
#include "Render/ShadowDebugWireRenderer.h"
#include "Render/SceneRendering/DeferredLightingPass.h"
#include "core/logger.h"
#include "core/wall_timer.h"

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

		/** UE Renderer-side: recycle scene-target surfaces at WxH + drop temporal history in post (TAA/SSR/Bloom, etc.). Invoked from render-thread lambdas only. */
		static void RecycleRenderTargetAndPostResource(FWorldSceneRenderPrivate* d, uint32_t W, uint32_t H)
		{
			if (!d || W == 0 || H == 0)
				return;
			if (d->SceneTextures)
				d->SceneTextures->InitDefaultSceneTargets(W, H);
			if (d->PostProcess)
				d->PostProcess->InvalidateTransientResources();
			if (d->SkylightEnvironment)
				d->SkylightEnvironment->InvalidateCapturedEnvironment();
		}

		/** Render thread: swapchain / viewport resolution + scene targets. */
		static void ApplyViewportResizeOnRenderThread(FWorldSceneRenderPrivate* Resources, uint32_t W, uint32_t H, bool bFullscreen)
		{
			if (!Resources || !Resources->MainViewPort || W == 0 || H == 0)
				return;
			Resources->MainViewPort->Resize(W, H, bFullscreen);
			RecycleRenderTargetAndPostResource(Resources, W, H);
		}
	} // namespace

	FWorldSceneRender::FWorldSceneRender(std::weak_ptr<World> Owner)
		: d_ptr(std::make_unique<FWorldSceneRenderPrivate>())
	{
		FWorldSceneRenderPrivate* d = d_ptr.get();
		d->Owner = std::move(Owner);
	}

	FWorldSceneRender::~FWorldSceneRender()
	{
		FlushRenderingCommands(ERenderQueueFlushCategory::LifetimeOrShutdown);
	}

	std::shared_ptr<World> FWorldSceneRender::GetWorld() const
	{
		const FWorldSceneRenderPrivate* d = d_ptr.get();
		return d->Owner.lock();
	}

	void FWorldSceneRender::SetWorld(std::weak_ptr<World> InWorld)
	{
		FWorldSceneRenderPrivate* d = d_ptr.get();
		d->Owner = std::move(InWorld);
	}

	void FWorldSceneRender::InitResource(std::shared_ptr<RHIViewPort> ViewPort)
	{
		FWorldSceneRenderPrivate* d = d_ptr.get();
		d->MainViewPort = ViewPort;

		auto SelfPin = shared_from_this();
		FWorldSceneRenderPrivate* const Resources = d;
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[SelfPin = std::move(SelfPin), Resources](RenderCore::DynamicRHI* RHI)
			{
				(void)SelfPin;
				FWorldSceneRenderPrivate* dLife = Resources;
				core::WallSplitTimer Wall;

				if (!dLife->SkylightEnvironment)
					dLife->SkylightEnvironment = std::make_shared<USkyLightComponent>(RHI);
				dLife->SkylightEnvironment->InitResource();
				const double MsSkylightEnv = Wall.split_ms();

				if (!dLife->PostProcess)
					dLife->PostProcess = std::make_shared<PostProcessor>(RHI);
				dLife->PostProcess->InitResource();
				const double MsPostProcess = Wall.split_ms();

				if (!dLife->SkyLightPass)
					dLife->SkyLightPass = std::make_shared<SkyLightRenderPass>(RHI);
				dLife->SkyLightPass->InitResource();
				const double MsSkyLightPass = Wall.split_ms();

				if (!dLife->SceneTextures)
					dLife->SceneTextures = std::make_shared<FSceneTextures>(RHI);
				auto Size = dLife->MainViewPort->GetSize();
				dLife->SceneTextures->InitDefaultSceneTargets(Size.cx, Size.cy);
				const double MsSceneTextures = Wall.split_ms();

				if (!dLife->ShadowRender)
					dLife->ShadowRender = std::make_shared<ShadowRenderPass>(RHI);
				dLife->ShadowRender->InitResource();
				const double MsShadow = Wall.split_ms();

				if (!dLife->ShadowDebugWire)
					dLife->ShadowDebugWire = std::make_shared<FShadowDebugWireRenderer>(RHI);
				dLife->ShadowDebugWire->InitResource();
				const double MsShadowDebug = Wall.split_ms();

				if (!dLife->DeferredLighting)
					dLife->DeferredLighting = std::make_shared<DeferredLightingPass>(RHI);
				dLife->DeferredLighting->InitResource();
				const double MsDeferredLighting = Wall.split_ms();

				dLife->IsInit = true;
				const double MsTotal = Wall.total_ms();
				core::inf() << core::perf::hdr(core::perf::kRenderRt, "WorldSceneRenderInit") << "total_ms=" << MsTotal << " skylight_env_ms=" << MsSkylightEnv
							<< " post_process_ms=" << MsPostProcess << " skylight_pass_ms=" << MsSkyLightPass
							<< " scene_textures_ms=" << MsSceneTextures << " shadow_ms=" << MsShadow << " shadow_debug_ms=" << MsShadowDebug
							<< " deferred_lighting_ms=" << MsDeferredLighting << "\n";
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
		auto SelfPin = shared_from_this();
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[SelfPin = std::move(SelfPin), d, Root](RenderCore::DynamicRHI* RHI)
			{
				(void)SelfPin;
				if (d->SkylightEnvironment)
					d->SkylightEnvironment->LoadConfig(Root);
				if (d->PostProcess)
					d->PostProcess->LoadConfig(Root);
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
		auto SelfPin = shared_from_this();
		FWorldSceneRenderPrivate* Resources = d_ptr.get();
		uint32_t W = InSizeX, H = InSizeY;
		bool bFs = bInIsFullscreen;
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[SelfPin = std::move(SelfPin), Resources, W, H, bFs](RenderCore::DynamicRHI* RHI)
			{
				(void)SelfPin;
				(void)RHI;
				ApplyViewportResizeOnRenderThread(Resources, W, H, bFs);
			});
	}

	void FWorldSceneRender::Render(float DeltaTime)
	{
		SubmitSceneForRendering(DeltaTime);
	}

	std::shared_ptr<USkyLightComponent> FWorldSceneRender::GetUSkyLightComponent() const
	{
		return d_ptr->SkylightEnvironment;
	}

	std::shared_ptr<PostProcessor> FWorldSceneRender::GetPostProcessor() const
	{
		return d_ptr->PostProcess;
	}

	std::shared_ptr<ShadowRenderPass> FWorldSceneRender::GetShadowRenderPass() const
	{
		return d_ptr->ShadowRender;
	}

	std::shared_ptr<RHIViewPort> FWorldSceneRender::GetViewPort() const
	{
		return d_ptr->MainViewPort;
	}

	void FWorldSceneRender::FlushClearMeshMaterialRenderCacheNow()
	{
		const std::shared_ptr<World> w = GetWorld();
		if (!w)
			return;
		const std::shared_ptr<FScene> scene = w->GetScene();
		if (!scene || !scene->GetMeshMaterialRenderCache())
			return;
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[scene](RenderCore::DynamicRHI* RHI)
			{
				(void)RHI;
				if (FMeshMaterialRenderCache* cache = scene->GetMeshMaterialRenderCache())
					cache->Clear();
			},
			false);
		FlushRenderingCommands(ERenderQueueFlushCategory::InvalidateRenderCaches);
	}

	void FWorldSceneRender::FlushClearShadowPassMeshCacheNow()
	{
		auto SelfPin = shared_from_this();
		FWorldSceneRenderPrivate* Resources = d_ptr.get();
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[Resources, SelfPin = std::move(SelfPin)](RenderCore::DynamicRHI* RHI)
			{
				(void)RHI;
				if (!Resources || !Resources->ShadowRender)
					return;
				Resources->ShadowRender->InvalidateCachedMainLightForShading();
				Resources->ShadowRender->ClearCachedMeshShadowPasses();
			},
			false);
		FlushRenderingCommands(ERenderQueueFlushCategory::InvalidateRenderCaches);
	}

	void FWorldSceneRender::NotifyWorldRenderingSceneChanged()
	{
		// UE analogue: batch renderer invalidation when the scene context behind the viewport changes (ReloadSceneJson tail).
		// View/camera temporal state: World::InvalidatePrimaryViewStateAfterSceneCut (FSceneViewState-like) via MainEngine::FinalizeViewportRenderingAfterSceneCut.
		FWorldSceneRenderPrivate* dLife = d_ptr.get();
		if (!dLife)
			return;

		core::vec2u Sz{};
		if (dLife->MainViewPort)
			Sz = dLife->MainViewPort->GetSize();

		auto SelfPin = shared_from_this();
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[SelfPin = std::move(SelfPin), dLife, Sz](RenderCore::DynamicRHI* RHI)
			{
				(void)SelfPin;
				(void)RHI;
				if (dLife->ShadowRender)
				{
					dLife->ShadowRender->InvalidateCachedMainLightForShading();
					dLife->ShadowRender->ClearCachedMeshShadowPasses();
				}
				RecycleRenderTargetAndPostResource(dLife, Sz.w, Sz.h);
			},
			false);
		FlushRenderingCommands(ERenderQueueFlushCategory::InvalidateRenderCaches);
	}

	void FWorldSceneRender::SetMaxSceneFramesInFlight(uint32_t MaxConcurrent) noexcept
	{
		d_ptr->MaxSceneFramesInFlight.store(MaxConcurrent, std::memory_order_relaxed);
	}

	uint64_t FWorldSceneRender::GetSubmissionSequence() const noexcept
	{
		return d_ptr ? d_ptr->SubmissionSequence.load(std::memory_order_relaxed) : 0ull;
	}

	uint32_t FWorldSceneRender::GetPendingSceneFramesCount() const noexcept
	{
		return d_ptr ? d_ptr->PendingSceneFrames.load(std::memory_order_relaxed) : 0u;
	}

	uint32_t FWorldSceneRender::GetMaxSceneFramesInFlight() const noexcept
	{
		return d_ptr ? d_ptr->MaxSceneFramesInFlight.load(std::memory_order_relaxed) : 0u;
	}

	void FWorldSceneRender::GetLastFramePassCpuTimings(std::vector<FRDGPassCpuTiming>& Out) const
	{
		Out.clear();
		if (!d_ptr)
			return;
		std::lock_guard<std::mutex> Lock(d_ptr->PassCpuTimingMutex);
		Out = d_ptr->LastFramePassCpuTimingsForGui;
	}

	void FWorldSceneRender::EndGameThreadFrameSync(bool bFlushRenderQueue, bool bGpuIdleWait)
	{
		if (bFlushRenderQueue)
			FlushRenderingCommands(ERenderQueueFlushCategory::PolicyEndOfTickRendersync);
		if (bGpuIdleWait)
		{
			if (GEngine)
				if (const auto Rhi = GEngine->GetRHI())
					Rhi->RHIWaitForGpuIdle();
		}
	}

	void FWorldSceneRender::SubmitSceneForRendering(float DeltaTime)
	{
		core::WallSplitTimer SubmitWall;
		auto RHI = GEngine->GetRHI();
		if (!RHI)
			return;
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
		Primary.BuildFromCamera(*World->GetMainCamera(), std::move(lightsSnapshot), bHaltonProjJitter, 0, 0,
								(int32_t)ViewFamily.RenderSizeX, (int32_t)ViewFamily.RenderSizeY);
		Primary.bUnlit = d->bUnlit;
		Primary.SkyLightIBLScale = World->GetSkyLightIBLScale();

		auto ViewDataPtr = std::make_shared<FSceneViewData>(Primary);
		std::shared_ptr<const FSceneViewData> ViewConst = ViewDataPtr;

		const std::shared_ptr<FScene> WorldScene = World->GetScene();
		if (!WorldScene)
			return;

		FPrimitiveGatherResult PrimitiveGather;
		FSceneRendererPrimitiveGather::GatherVisiblePrimitives(*ViewConst, *WorldScene, PrimitiveGather);

		std::vector<GltfSceneMeshInfo> MeshesInfoCopy = std::move(PrimitiveGather.VisiblePrimitives);
		std::vector<GltfSceneMeshInfo> shadowCasters = std::move(PrimitiveGather.DynamicShadowCastingPrimitives);
		std::vector<GltfSceneMeshInfo> shadowFrustumBounds = std::move(PrimitiveGather.ShadowFrustumCullPrimitives);

		FShadowProjectorSceneData shadowProjectorScene = World->BuildShadowProjectorAggregateData();
		shadowProjectorScene.ViewWorldBoundsAabb = ViewConst->ViewFrustum.bbox;
		shadowProjectorScene.bHasViewWorldBoundsForDirectionalReceiverXY = true;

		shadowProjectorScene.bHasCascadeCameraParams = true;
		shadowProjectorScene.CameraView = ViewConst->ViewMatrix;
		shadowProjectorScene.CameraWorldPos = ViewConst->CameraPos;
		shadowProjectorScene.CameraNearZ = ViewConst->CameraNearZ;
		shadowProjectorScene.CameraFarZ = ViewConst->CameraFarZ;
		shadowProjectorScene.CameraAspectWH =
			ViewFamily.RenderSizeY > 0 ? static_cast<float>(ViewFamily.RenderSizeX) / static_cast<float>(ViewFamily.RenderSizeY) : 1.f;
		if (World->GetMainCamera())
			shadowProjectorScene.CameraFovYRad = World->GetMainCamera()->GetFovVerticalRadians();
		// CSM split metric uses dot(worldPos - cam, fwd); fwd MUST match view matrix depth axis (same as WorldBoundsFromViewProjSliceInverse / NearZ–FarZ splits).
		// Frustum corner averages can diverge slightly from MatrixLookAtLH forward → wrong cascade index → blocky/wrong shadow sampling.
		{
			const math::Matrix4x4& V = ViewConst->ViewMatrix;
			math::Vector3 fwd(V._02, V._12, V._22);
			if (fwd.GetSqrLength() > 1e-12f)
				fwd.Normalize();
			else
				fwd = math::Vector3(0.f, 0.f, 1.f);
			shadowProjectorScene.CameraForwardWorld = fwd;
		}

		std::vector<Light> shadowLights(ViewConst->Lights.begin(), ViewConst->Lights.end());

		FSkyLightSourceDesc skyLightSrc = World->ResolvePrimarySkyLightSource();

		{
			const uint32_t cap = d->MaxSceneFramesInFlight.load(std::memory_order_relaxed);
			uint32_t throttleFlushIters = 0;
			core::WallSplitTimer ThrottleWall;
			while (cap > 0u)
			{
				const uint32_t pending = d->PendingSceneFrames.load(std::memory_order_relaxed);
				if (pending < cap)
					break;
				FlushRenderingCommands(ERenderQueueFlushCategory::ThrottleQueuedSceneFrames);
				++throttleFlushIters;
			}
			if (throttleFlushIters > 0)
			{
				const double throttleMs = ThrottleWall.total_ms();
				core::inf() << core::perf::hdr(core::perf::kTick, "SubmitSceneThrottle") << "max_in_flight=" << cap << " flush_calls=" << throttleFlushIters
							<< " blocked_ms=" << throttleMs
							<< " note=game_thread_wait_render_queue_see_Perf|render_rec|ExecuteFrame cli_maxrenderframes_le0_disables_cap\n";
			}

			std::lock_guard<std::mutex> FrameLock(d->RenderFrameMutex);
			FSceneRenderPacket Packet{};
			Packet.WorldSceneRenderOwner = this;
			Packet.SceneResources = d;
			Packet.WorldScene = WorldScene;
			Packet.ViewData = ViewConst;
			Packet.MeshesInfo = std::move(MeshesInfoCopy);
			Packet.ShadowCasters = std::move(shadowCasters);
			Packet.ShadowFrustumBounds = std::move(shadowFrustumBounds);
			Packet.LightsForShadow = std::move(shadowLights);
			Packet.ShadowProjectorScene = shadowProjectorScene;
			Packet.SkyLightSource = std::move(skyLightSrc);
			Packet.SubmissionSequence =
				static_cast<uint64_t>(d->SubmissionSequence.fetch_add(1u, std::memory_order_relaxed)) + 1ull;

			const std::shared_ptr<FSceneRenderPacket> Job = std::make_shared<FSceneRenderPacket>(std::move(Packet));
			d->PendingSceneFrames.fetch_add(1u, std::memory_order_relaxed);
			std::atomic<uint32_t>* const pendingPtr = &d->PendingSceneFrames;
			ENQUEUE_UNIQUE_RENDER_COMMAND(
				[Job, pendingPtr](DynamicRHI* RHIIn)
				{
					struct PendingDecrement
					{
						std::atomic<uint32_t>* P = nullptr;
						~PendingDecrement()
						{
							if (P)
								P->fetch_sub(1u, std::memory_order_relaxed);
						}
					} dec{ pendingPtr };
					if (!Job || !Job->SceneResources)
						return;
					Job->SceneResources->SceneRenderer.ExecuteFrame(RHIIn, std::move(*Job));
				},
				false);
		}

		const double submitMs = SubmitWall.total_ms();
		if (submitMs >= 50.0)
			core::inf() << core::perf::hdr(core::perf::kTick, "SubmitSceneForRendering") << "wall_ms=" << submitMs
						 << " note=gather_throttle_enqueue_async_Perf|render_rec|ExecuteFrame\n";

		// Default: no per-tick Flush; maxrenderframes throttles via Flush; rendersync/gpuwait at end of MainEngine::Tick.
	}

} // namespace Engine
