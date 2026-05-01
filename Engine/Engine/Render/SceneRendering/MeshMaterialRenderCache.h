#pragma once
#include "Render/MaterialRender.h"
#include <memory>
#include <unordered_map>

namespace Engine
{
	class MeshBase;

	/** Caches material draw instances keyed by mesh buffer and material asset to reduce pipeline state churn. */
	class FMeshMaterialRenderCache
	{
	public:
		std::shared_ptr<MaterialRender> GetOrCreate(std::shared_ptr<MeshBase> Mesh);

	private:
		struct FMaterialRenderCacheKey
		{
			const void* MeshBuffer = nullptr;
			const void* Material = nullptr;
			bool operator==(const FMaterialRenderCacheKey& Other) const noexcept
			{
				return MeshBuffer == Other.MeshBuffer && Material == Other.Material;
			}
		};

		struct FMaterialRenderCacheKeyHash
		{
			size_t operator()(const FMaterialRenderCacheKey& Key) const noexcept
			{
				const uintptr_t A = reinterpret_cast<uintptr_t>(Key.MeshBuffer);
				const uintptr_t B = reinterpret_cast<uintptr_t>(Key.Material);
				return A ^ (B + 0x9e3779b9ull + (A << 6) + (A >> 2));
			}
		};

		std::unordered_map<FMaterialRenderCacheKey, std::shared_ptr<MaterialRender>, FMaterialRenderCacheKeyHash> CachedRenders;
	};
}
