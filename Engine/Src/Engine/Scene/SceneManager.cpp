#include "Scene/SceneManager.h"
#include "Scene/World.h"
#include "Scene/GameViewportClient.h"
#include "Render/WorldSceneRender.h"
#include "Render/Shadow/ShadowRenderPass.h"
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

		// Drain render-thread recording, then GPU idle before tearing down old World's textures/buffers (D3D12: use-after-free shows as rectangular garbage).
		FlushRenderingCommands();
		OwnerEngine_->GetRHI()->Wait();

		if (const auto sr = SceneRender_.lock())
		{
			sr->FlushClearMeshMaterialRenderCacheNow();
			// Shadow pass caches ShadowPS per Mesh shared_ptr; keeping entries pins old meshes across World reset (BS-only scenes → Model3 swap leaked skeleton/layout state).
			if (const auto ShadowPass = sr->GetShadowRenderPass())
				ShadowPass->ClearCachedMeshShadowPasses();
		}

		UnbindInvalidateFromCurrentWorld();

		auto newWorld = std::make_shared<World>();
		vc->SetWorldWeak(std::weak_ptr<World>(newWorld));
		sr->SetWorldWeak(std::weak_ptr<World>(newWorld));

		std::shared_ptr<World> oldWorld = World_;
		World_ = std::move(newWorld);

		BindInvalidateToCurrentWorld();

		oldWorld.reset();

		if (World_)
			World_->LoadScene(JsonPath);
		if (World_)
			World_->ApplySceneTransitionPrimaryCameraState();
		if (sr)
			sr->RequestRenderingResetAfterSceneTransition();

		// Blend-shape scenes enqueue UpdateVert every frame; after a swap, flushing the render-thread queue does not mean the GPU is done.
		// RequestRenderingResetAfterSceneTransition already flushes; wait for idle so the first frame on the new scene cannot latch stale submits or unready VBs.
		if (OwnerEngine_->GetRHI())
			OwnerEngine_->GetRHI()->Wait();
	}

}
