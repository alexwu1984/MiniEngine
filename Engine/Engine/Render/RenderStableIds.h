#pragma once
#include <atomic>
#include <cstdint>

namespace Engine
{
	/** Process-wide monotonic ids so render caches never alias recycled heap pointers across Worlds. */

	inline uint64_t AllocateActorStableInstanceId() noexcept
	{
		static std::atomic<uint64_t> Next{1};
		return Next.fetch_add(1u, std::memory_order_relaxed);
	}

	inline uint64_t AllocateComponentStableInstanceId() noexcept
	{
		static std::atomic<uint64_t> Next{1};
		return Next.fetch_add(1u, std::memory_order_relaxed);
	}

	inline uint64_t AllocateMaterialStableInstanceId() noexcept
	{
		static std::atomic<uint64_t> Next{1};
		return Next.fetch_add(1u, std::memory_order_relaxed);
	}

	inline uint64_t MixStableRenderId(uint64_t A, uint64_t B) noexcept
	{
		return A ^ (B + 0x9e3779b97f4a7c15ULL + (A << 6) + (A >> 2));
	}

	/** One drawable submesh slot within the current scene lifetime (stable vs raw MeshBuffer/Material pointers). */
	inline uint64_t BuildMeshMaterialRenderCacheKey(uint64_t ActorStableId, uint64_t ComponentStableId, uint32_t MeshOrdinalWithinComponent,
													 uint64_t MaterialStableInstanceId) noexcept
	{
		uint64_t K = MixStableRenderId(ActorStableId, ComponentStableId);
		K = MixStableRenderId(K, static_cast<uint64_t>(MeshOrdinalWithinComponent));
		K = MixStableRenderId(K, MaterialStableInstanceId);
		return K;
	}
}
