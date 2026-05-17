#include "Scene/FScene.h"
#include "Scene/SceneMeshComponent.h"
#include "Scene/Actor.h"
#include "Render/SceneRendering/MeshMaterialRenderCache.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "Render/SceneRendering/SceneRendererPrimitiveGather.h"
#include "Thread/RenderThread.h"

namespace Engine
{
	FScene::FScene()
		: MeshMaterialRenderCache(std::make_unique<FMeshMaterialRenderCache>())
	{
	}

	FScene::~FScene() = default;

	FPrimitiveSceneProxy::FPrimitiveSceneProxy(std::weak_ptr<SceneMeshComponent> InMesh)
		: MeshWeak(std::move(InMesh))
	{
	}

	std::shared_ptr<SceneMeshComponent> FPrimitiveSceneProxy::LockMesh() const
	{
		return MeshWeak.lock();
	}

	void FPrimitiveSceneProxy::AppendForView(const FSceneViewData& ViewData, FPrimitiveGatherResult& OutResult) const
	{
		const std::shared_ptr<SceneMeshComponent> Comp = MeshWeak.lock();
		if (!Comp)
			return;
		const std::shared_ptr<Actor> ActorOwner = Comp->GetOwner();
		if (!ActorOwner || ActorOwner->GetState() != Actor::EActive || !ActorOwner->IsVisible())
			return;

		if (Comp->IsProjectShadow())
		{
			GltfSceneMeshInfo UnculledCaster;
			if (Comp->GatherMesh(UnculledCaster, nullptr))
				OutResult.DynamicShadowCastingPrimitives.push_back(std::move(UnculledCaster));
		}

		GltfSceneMeshInfo SceneMeshInfo;
		if (!Comp->GatherMesh(SceneMeshInfo, &ViewData.ViewFrustum))
			return;
		OutResult.ShadowFrustumCullPrimitives.push_back(SceneMeshInfo);
		OutResult.VisiblePrimitives.push_back(std::move(SceneMeshInfo));
	}

	void FScene::AddScenePrimitive(const std::shared_ptr<SceneMeshComponent>& MeshComp)
	{
		if (!MeshComp)
			return;
		std::lock_guard<std::mutex> Lock(Mutex);
		Primitives.push_back(std::make_shared<FPrimitiveSceneProxy>(std::weak_ptr<SceneMeshComponent>(MeshComp)));
	}

	void FScene::RemoveScenePrimitive(const std::shared_ptr<SceneMeshComponent>& MeshComp)
	{
		if (!MeshComp)
			return;
		const std::vector<uint64_t> SlotKeys = MeshComp->BuildMeshMaterialRenderCacheStableSlotKeys();
		if (!SlotKeys.empty())
		{
			std::weak_ptr<FScene> WeakLife = weak_from_this();
			ENQUEUE_UNIQUE_RENDER_COMMAND(
				[WeakLife, SlotKeys](RenderCore::DynamicRHI* RHI)
				{
					(void)RHI;
					std::shared_ptr<FScene> S = WeakLife.lock();
					if (!S)
						return;
					FMeshMaterialRenderCache* const Cache = S->GetMeshMaterialRenderCache();
					if (!Cache)
						return;
					for (const uint64_t K : SlotKeys)
					{
						if (K != 0)
							Cache->InvalidateByStableSlotKey(K);
					}
				},
				false);
		}
		std::lock_guard<std::mutex> Lock(Mutex);
		Primitives.erase(std::remove_if(Primitives.begin(), Primitives.end(),
										 [&](const std::shared_ptr<FPrimitiveSceneProxy>& P) { return P->LockMesh() == MeshComp; }),
						 Primitives.end());
	}

	void FScene::ClearScenePrimitives()
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		Primitives.clear();
	}

	std::vector<std::shared_ptr<FPrimitiveSceneProxy>> FScene::SnapshotPrimitives() const
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		return Primitives;
	}

	FMeshMaterialRenderCache* FScene::GetMeshMaterialRenderCache() noexcept
	{
		return MeshMaterialRenderCache.get();
	}

	const FMeshMaterialRenderCache* FScene::GetMeshMaterialRenderCache() const noexcept
	{
		return MeshMaterialRenderCache.get();
	}
} // namespace Engine
