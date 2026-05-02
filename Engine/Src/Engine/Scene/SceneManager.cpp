#include "Scene/SceneManager.h"
#include "Scene/World.h"
#include "Scene/GameViewportClient.h"
#include "Render/WorldSceneRender.h"
#include "Engine/Engine.h"
#include "Thread/RenderThread.h"
#include "RHI/DynamicRHI.h"

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

		// --- ReplaceWorld + new Json ---
		// 1) Drain + GPU idle: safe to destroy old World's GPU resources (D3D12 UAF → garbage rectangles).
		FlushRenderingCommands(ERenderQueueFlushCategory::ReloadOrWorldSwap);
		OwnerEngine_->GetRHI()->RHIWaitForGpuIdle();

		// 2) UE426-style: flush mesh-material draw cache only when replacing World (pointer-keyed caches; before ~FScene).
		//    Shadow/mesh caches on FWorldSceneRender: FinalizeViewportRenderingAfterSceneCut.
		if (const auto srFlush = SceneRender_.lock())
			srFlush->FlushClearMeshMaterialRenderCacheNow();

		// 3) New World; viewport weak ref + clear queued input (old roam must not move new camera).
		auto newWorld = std::make_shared<World>();
		vc->SetWorld(std::weak_ptr<World>(newWorld));
		vc->ClearPendingInput();

		std::shared_ptr<World> oldWorld = World_;
		World_ = std::move(newWorld);
		oldWorld.reset();

		// 4) Rebind long-lived FWorldSceneRender -> new World; FScene/primitive lifetimes belong to World.
		OwnerEngine_->RebindSceneRenderToCurrentWorld();

		// 5) Load Json (may enqueue IBL / pre / post on render thread).
		if (World_)
			World_->LoadScene(JsonPath);
		FlushRenderingCommands(ERenderQueueFlushCategory::ReloadOrWorldSwap);

		// 6) UE split: ViewState + Renderer transient invalidation batch.
		if (OwnerEngine_)
			OwnerEngine_->FinalizeViewportRenderingAfterSceneCut();

		OwnerEngine_->GetRHI()->RHIWaitForGpuIdle();
	}
}
