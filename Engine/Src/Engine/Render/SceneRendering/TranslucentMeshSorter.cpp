#include "Render/SceneRendering/TranslucentMeshSorter.h"
#include "GltfModel/GltfMesh.h"

namespace Engine
{
	void FTranslucentMeshSorter::AppendPerActorMeshSortKeys(const GltfSceneMeshInfo& SceneMeshInfo, const math::Vector3& CameraWorldPosition,
															std::vector<FTranslucentMeshSortKey>& OutKeys)
	{
		const size_t MeshCount = SceneMeshInfo.Meshes.size();
		for (int32_t MeshIndex = 0; MeshIndex < static_cast<int32_t>(MeshCount); ++MeshIndex)
		{
			FTranslucentMeshSortKey Key;
			std::shared_ptr<MeshBase> Mesh = SceneMeshInfo.Meshes[MeshIndex];

			math::Vector3 BoxPoint[8]{};
			Mesh->GetBoundingBox().GetPoint(BoxPoint);

			float DistanceMin = 100000.f;
			float DistanceMax = 0.f;
			Key.WorldTransform = SceneMeshInfo.WorldTransform;
			Key.PrevWorldTransform = SceneMeshInfo.PrevWorldTransform;
			Key.Mesh = Mesh;

			for (int32_t PointIndex = 0; PointIndex < 8; PointIndex++)
			{
				const math::Vector3& Point = BoxPoint[PointIndex];
				math::Vector4 Point4(Point.x, Point.y, Point.z, 1.0f);
				math::Vector4 TargetPoint = Point4 * Mesh->GetMeshMat();
				TargetPoint = TargetPoint / TargetPoint.w;

				const float Distance = math::Abs(TargetPoint.z - CameraWorldPosition.z);

				if (Distance < DistanceMin)
				{
					DistanceMin = Distance;
					Key.SortDistance = DistanceMin;
				}
				if (Distance > DistanceMax)
				{
					DistanceMax = Distance;
					Key.SortDistance = DistanceMax;
				}
			}
			OutKeys.push_back(Key);
		}
	}

	void FTranslucentMeshSorter::AppendSceneSortKeys(const std::vector<GltfSceneMeshInfo>& SceneMeshInfos, const math::Vector3& CameraWorldPosition,
													 std::vector<FTranslucentMeshSortKey>& OutKeys)
	{
		OutKeys.clear();
		for (const auto& SceneMeshInfo : SceneMeshInfos)
		{
			AppendPerActorMeshSortKeys(SceneMeshInfo, CameraWorldPosition, OutKeys);
		}
		std::sort(OutKeys.begin(), OutKeys.end(), FTranslucentMeshSortKey());
	}

	void FTranslucentMeshSorter::SortByDistance(std::vector<FTranslucentMeshSortKey>& InOutKeys)
	{
		std::sort(InOutKeys.begin(), InOutKeys.end(), FTranslucentMeshSortKey());
	}
}
