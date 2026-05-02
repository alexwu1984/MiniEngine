#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

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
	 */
	class FScene
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

		/** Game thread: next Render on a frame that captures this FScene clears the cache (raw pointer keys). */
		void RequestMeshMaterialRenderCacheInvalidate() noexcept;
		/** Render thread: if true, caller should Clear() then treat as consumed. */
		bool ConsumeMeshMaterialCacheInvalidatePending() noexcept;
		void ClearMeshMaterialCacheInvalidatePending() noexcept;

	private:
		mutable std::mutex Mutex;
		std::vector<std::shared_ptr<FPrimitiveSceneProxy>> Primitives;

		std::unique_ptr<FMeshMaterialRenderCache> MeshMaterialRenderCache;
		std::atomic_bool bMeshMaterialCacheInvalidatePending{ false };
	};
} // namespace Engine
