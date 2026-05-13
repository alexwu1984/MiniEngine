#include "Scene/SceneManager.h"
#include "Scene/World.h"
#include "Scene/GameViewportClient.h"
#include "Render/WorldSceneRender.h"
#include "Engine/Engine.h"
#include "Thread/RenderThread.h"
#include "RHI/DynamicRHI.h"
#include "core/logger.h"
#include "core/strings.h"
#include "core/wall_timer.h"

namespace Engine
{

	SceneManager::SceneManager()
		: World_(std::make_shared<World>())
	{
	}

	void SceneManager::AttachClients(MainEngine* OwnerEngine,
									 const std::shared_ptr<GameViewportClient>& ViewportClient,
									 const std::shared_ptr<FWorldSceneRender>& SceneRender)
	{
		OwnerEngine_ = OwnerEngine;
		if (ViewportClient)
			ViewportClient_ = ViewportClient;
		if (SceneRender)
			SceneRender_ = SceneRender;
	}

	void SceneManager::ReloadSceneJson(const std::wstring& JsonPath)
	{
		if (!OwnerEngine_)
			return;
		const auto vc = ViewportClient_.lock();
		const auto sr = SceneRender_.lock();
		if (!OwnerEngine_->GetRHI() || !vc || !sr)
			return;

		core::WallSplitTimer Wall;
		double MsFlush0 = 0., MsGpuIdle0 = 0., MsCacheFlush = 0., MsWorldSwapRebind = 0., MsLoadScene = 0., MsFlush1 = 0.,
			MsFinalize = 0., MsGpuIdle1 = 0.;

		// --- ReplaceWorld + new Json ---
		// 1) Drain + GPU idle: safe to destroy old World's GPU resources (D3D12 UAF / garbage rects).
		FlushRenderingCommands(ERenderQueueFlushCategory::ReloadOrWorldSwap);
		MsFlush0 = Wall.split_ms();
		OwnerEngine_->GetRHI()->RHIWaitForGpuIdle();
		MsGpuIdle0 = Wall.split_ms();

		// 2) Drop shadow/mesh caches that key on MeshBase* before ~World (shadow pass map otherwise keeps old meshes + ShadowPS alive).
		if (const auto srFlush = SceneRender_.lock())
		{
			srFlush->FlushClearShadowPassMeshCacheNow();
			srFlush->FlushClearMeshMaterialRenderCacheNow();
		}
		MsCacheFlush = Wall.split_ms();

		// 3) New World; viewport weak ref + clear queued input (old roam must not move new camera).
		auto newWorld = std::make_shared<World>();
		vc->SetWorld(std::weak_ptr<World>(newWorld));
		vc->ClearPendingInput();

		std::shared_ptr<World> oldWorld = World_;
		World_ = std::move(newWorld);
		oldWorld.reset();

		// 4) Rebind long-lived FWorldSceneRender -> new World; FScene/primitive lifetimes belong to World.
		OwnerEngine_->RebindSceneRenderToCurrentWorld();
		MsWorldSwapRebind = Wall.split_ms();

		// 5) Load Json (may enqueue IBL / pre / post on render thread).
		if (World_)
			World_->LoadScene(JsonPath);
		MsLoadScene = Wall.split_ms();

		FlushRenderingCommands(ERenderQueueFlushCategory::ReloadOrWorldSwap);
		MsFlush1 = Wall.split_ms();

		// 6) UE split: ViewState + Renderer transient invalidation batch.
		if (OwnerEngine_)
			OwnerEngine_->FinalizeViewportRenderingAfterSceneCut();
		MsFinalize = Wall.split_ms();

		OwnerEngine_->GetRHI()->RHIWaitForGpuIdle();
		MsGpuIdle1 = Wall.split_ms();

		const double MsTotal =
			MsFlush0 + MsGpuIdle0 + MsCacheFlush + MsWorldSwapRebind + MsLoadScene + MsFlush1 + MsFinalize + MsGpuIdle1;
		if (core::perf::ShouldEmitPerfInfLogs())
		{
			core::inf() << core::perf::hdr(core::perf::kScene, "ReloadSceneJson") << "total_ms=" << MsTotal << " path_utf8=" << core::ucs2_u8(JsonPath)
						<< " flush0_ms=" << MsFlush0 << " gpu_idle0_ms=" << MsGpuIdle0 << " cache_clear_ms=" << MsCacheFlush
						<< " world_swap_rebind_ms=" << MsWorldSwapRebind << " load_scene_ms=" << MsLoadScene
						<< " flush1_ms=" << MsFlush1 << " finalize_scene_cut_ms=" << MsFinalize << " gpu_idle1_ms=" << MsGpuIdle1 << "\n";
		}
	}
}
