#pragma once
#include "core/inc.h"

namespace Engine
{
	class SceneMeshComponent;
	class FMeshMaterialRenderCache;
	struct FSceneViewData;
	struct FPrimitiveGatherResult;

	/**
	 * UE4-style minimal primitive scene proxy: render-side entry for one mesh component, created on the game thread.
	 * Full scene swap destroys the owning FScene with World — no global primitive table.
	 */
	class FPrimitiveSceneProxy
	{
	public:
		explicit FPrimitiveSceneProxy(std::weak_ptr<SceneMeshComponent> InMesh);

		std::shared_ptr<SceneMeshComponent> LockMesh() const;

		/** Per-view classification: visible list, shadow caster subset, shadow frustum bounds (same rules as legacy actor scan). */
		void AppendForView(const FSceneViewData& ViewData, FPrimitiveGatherResult& OutResult) const;

	private:
		std::weak_ptr<SceneMeshComponent> MeshWeak;
	};

	/**
	 * UE4 FScene subset: primitive proxies + scene-local mesh/material draw cache (lifetime = this World).
	 * Created only via std::make_shared (World owns); enables weak_from_this for render-thread lifetime when enqueuing cache invalidates.
	 */
	class FScene : public std::enable_shared_from_this<FScene>
	{
	public:
		FScene();
		~FScene();

		void AddScenePrimitive(const std::shared_ptr<SceneMeshComponent>& MeshComp);
		void RemoveScenePrimitive(const std::shared_ptr<SceneMeshComponent>& MeshComp);
		void ClearScenePrimitives();

		/** Caller: game thread. Short lock to copy proxy list; then culls without holding mutex. */
		std::vector<std::shared_ptr<FPrimitiveSceneProxy>> SnapshotPrimitives() const;

		FMeshMaterialRenderCache* GetMeshMaterialRenderCache() noexcept;
		const FMeshMaterialRenderCache* GetMeshMaterialRenderCache() const noexcept;

	private:
		mutable std::mutex Mutex;
		std::vector<std::shared_ptr<FPrimitiveSceneProxy>> Primitives;

		std::unique_ptr<FMeshMaterialRenderCache> MeshMaterialRenderCache;
	};
} // namespace Engine
