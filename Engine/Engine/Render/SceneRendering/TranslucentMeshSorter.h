#pragma once
#include "Render/SceneRendering/TranslucentMeshSortKey.h"
#include "Scene/SceneMeshComponent.h"
#include "math/vector3.h"
#include <vector>

namespace Engine
{
	/** Builds and sorts mesh keys for translucent draw ordering. */
	class FTranslucentMeshSorter
	{
	public:
		static void AppendPerActorMeshSortKeys(const GltfSceneMeshInfo& SceneMeshInfo, const math::Vector3& CameraWorldPosition,
											   std::vector<FTranslucentMeshSortKey>& OutKeys);
		static void AppendSceneSortKeys(const std::vector<GltfSceneMeshInfo>& SceneMeshInfos, const math::Vector3& CameraWorldPosition,
										 std::vector<FTranslucentMeshSortKey>& OutKeys);
		static void SortByDistance(std::vector<FTranslucentMeshSortKey>& InOutKeys);
	};
}
