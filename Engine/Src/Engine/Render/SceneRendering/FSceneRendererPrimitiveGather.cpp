#include "Render/SceneRendering/FSceneRendererPrimitiveGather.h"
#include "Render/SceneRendering/FSceneViewData.h"
#include "Scene/Actor.h"
#include "Scene/GltfMeshComponent.h"

namespace Engine
{
	void FSceneRendererPrimitiveGather::GatherVisiblePrimitives(const FSceneViewData& ViewData, const std::vector<std::shared_ptr<Actor>>& Actors,
																FPrimitiveGatherResult& OutResult)
	{
		OutResult.VisiblePrimitives.clear();
		OutResult.DynamicShadowCastingPrimitives.clear();
		OutResult.ShadowFrustumCullPrimitives.clear();

		const std::size_t ActorCount = Actors.size();
		const std::size_t ReserveHint = ActorCount * 4u + 8u;
		OutResult.VisiblePrimitives.reserve(ReserveHint);
		OutResult.DynamicShadowCastingPrimitives.reserve(ActorCount + 4u);
		OutResult.ShadowFrustumCullPrimitives.reserve(ReserveHint);

		const math::Frustum& CullFrustum = ViewData.ViewFrustum;

		for (const auto& ActorItem : Actors)
		{
			if (!ActorItem || ActorItem->GetState() != Actor::EActive || !ActorItem->IsVisible())
				continue;

			auto Components = std::move(ActorItem->GetComponents<GltfMeshComponent>());
			for (auto& ComponentItem : Components)
			{
				GltfSceneMeshInfo SceneMeshInfo;
				if (!ComponentItem->GatherMesh(SceneMeshInfo, CullFrustum))
					continue;

				OutResult.ShadowFrustumCullPrimitives.push_back(SceneMeshInfo);
				if (ComponentItem->IsProjectShadow())
					OutResult.DynamicShadowCastingPrimitives.push_back(SceneMeshInfo);

				OutResult.VisiblePrimitives.push_back(std::move(SceneMeshInfo));
			}
		}
	}
}
