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

	void SceneManager::BindInvalidateToCurrentWorld()
	{
		if (!OwnerEngine_ || !World_)
			return;
		const auto sr = SceneRender_.lock();
		if (!sr)
			return;
		World_->sigSceneActorRenderResourcesInvalidated.bind(
			std::function<void()>([wpSceneRender = std::weak_ptr<FWorldSceneRender>(sr)]() {
				if (const auto s = wpSceneRender.lock())
					s->RequestMeshMaterialRenderCacheInvalidate();
			}),
			OwnerEngine_);
	}

	void SceneManager::UnbindInvalidateFromCurrentWorld()
	{
		if (!OwnerEngine_ || !World_)
			return;
		World_->sigSceneActorRenderResourcesInvalidated.unbind(OwnerEngine_);
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
		FlushRenderingCommands();
		OwnerEngine_->GetRHI()->Wait();

		// 2) Old World's FScene: clear mesh-material cache on the render thread before ~World.
		//    (Avoids many MaterialRender::~ each WaitForFinish on the game thread when the map is torn down.)
		//    Shadow mesh→ShadowPS map lives on FWorldSceneRender and is released in ~ShadowRenderPass — no manual clear here.
		if (const auto sr = SceneRender_.lock())
			sr->FlushClearMeshMaterialRenderCacheNow();

		// 3) Old World's render-invalidate delegate.
		UnbindInvalidateFromCurrentWorld();

		// 4) New World; viewport weak ref + clear queued input (old roam must not move new camera).
		auto newWorld = std::make_shared<World>();
		vc->SetWorldWeak(std::weak_ptr<World>(newWorld));
		vc->ClearPendingInput();

		std::shared_ptr<World> oldWorld = World_;
		World_ = std::move(newWorld);
		oldWorld.reset();

		// 5) New FWorldSceneRender (new shadow pass / RDG targets path; mesh-material cache is on new World's FScene). Rebind invalidate.
		OwnerEngine_->RecreateWorldSceneRenderForSceneSwap();
		BindInvalidateToCurrentWorld();

		// 6) Load Json (may enqueue IBL / pre / post on render thread).
		if (World_)
			World_->LoadScene(JsonPath);
		FlushRenderingCommands();

		// 7) Primary camera: mark temporal/history stale (pairs post-process reset in step 8).
		if (World_)
			World_->ApplySceneTransitionPrimaryCameraState();

		// 8) InitDefaultSceneTargets + InvalidateTransientResources (TAA/SSR/Bloom temporals); see WorldSceneRender.cpp.
		if (const auto newSr = OwnerEngine_->GetSceneRender())
			newSr->RequestRenderingResetAfterSceneTransition();

		OwnerEngine_->GetRHI()->Wait();
	}
}
