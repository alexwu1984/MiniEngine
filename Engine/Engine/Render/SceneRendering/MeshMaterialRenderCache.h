#pragma once
#include "Render/MaterialRender.h"
#include "Render/RenderStableIds.h"
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace Engine
{
	class MeshBase;

	/** Stable logical slot + GPU/resource identities — avoids lone-hash collisions and stale pointer reuse. */
	struct FMaterialRenderCacheLookupKey
	{
		uint64_t StableSlotKey = 0;
		uintptr_t MeshBuffer = 0;
		uintptr_t Material = 0;
		/** GltfMeshBuffer::DeclaredVertexFeatures — avoids hits when heap reuses MeshBuffer address after BS→scene swap with different IL/macros. */
		uint32_t DeclaredVtxFeat = 0;
		/** FWorldSceneRender::MeshMaterialCacheSceneGeneration — invalidates cache across full World replacement even if pointers recycle. */
		uint64_t SceneGeneration = 0;

		bool operator==(const FMaterialRenderCacheLookupKey& O) const noexcept
		{
			return StableSlotKey == O.StableSlotKey && MeshBuffer == O.MeshBuffer && Material == O.Material && DeclaredVtxFeat == O.DeclaredVtxFeat
				&& SceneGeneration == O.SceneGeneration;
		}
	};

	struct FMaterialRenderCacheLookupKeyHash
	{
		size_t operator()(const FMaterialRenderCacheLookupKey& K) const noexcept
		{
			uint64_t H = MixStableRenderId(K.StableSlotKey, static_cast<uint64_t>(K.MeshBuffer));
			H = MixStableRenderId(H, static_cast<uint64_t>(K.Material));
			H = MixStableRenderId(H, static_cast<uint64_t>(K.DeclaredVtxFeat));
			H = MixStableRenderId(H, K.SceneGeneration);
			return static_cast<size_t>(H);
		}
	};

	/** Caches MaterialRender instances for deferred base passes. */
	class FMeshMaterialRenderCache
	{
	public:
		std::shared_ptr<MaterialRender> GetOrCreate(std::shared_ptr<MeshBase> Mesh, uint64_t StableMaterialRenderCacheKey, uint64_t SceneMaterialCacheGeneration);
		void Clear() noexcept { CachedRenders.clear(); }

	private:
		std::unordered_map<FMaterialRenderCacheLookupKey, std::shared_ptr<MaterialRender>, FMaterialRenderCacheLookupKeyHash> CachedRenders;
	};
}
