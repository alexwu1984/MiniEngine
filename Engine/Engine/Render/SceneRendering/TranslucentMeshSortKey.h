#pragma once
#include "math/matrix4x4.h"

namespace Engine
{
	class MeshBase;

	/** Distance key used when ordering translucent draws from approximate projected bounds. */
	struct FTranslucentMeshSortKey
	{
		float SortDistance = 0.f;
		std::shared_ptr<MeshBase> Mesh;
		uint64_t MaterialRenderCacheKey = 0;
		math::Matrix4x4 WorldTransform;
		math::Matrix4x4 PrevWorldTransform;

		bool operator()(const FTranslucentMeshSortKey& Near, const FTranslucentMeshSortKey& Far) const
		{
			return Near.SortDistance < Far.SortDistance;
		}
	};
}
