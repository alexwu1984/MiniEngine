#include "Render/SceneRendering/FSceneRendererPrimitiveGather.h"
#include "Scene/SceneView.h"
#include "Scene/Actor.h"
#include "Scene/CameraComponent.h"
#include "Scene/GltfMeshComponent.h"

namespace Engine
{
	void FSceneRendererPrimitiveGather::GatherVisiblePrimitives(const SceneView& View, FPrimitiveGatherResult& OutResult)
	{
		OutResult.VisiblePrimitives.clear();
		OutResult.DynamicShadowCastingPrimitives.clear();
		OutResult.ShadowFrustumCullPrimitives.clear();

		const auto& Actors = View.GetAllActors();
		const std::size_t ActorCount = Actors.size();
		const std::size_t ReserveHint = ActorCount * 4u + 8u;
		OutResult.VisiblePrimitives.reserve(ReserveHint);
		OutResult.DynamicShadowCastingPrimitives.reserve(ActorCount + 4u);
		OutResult.ShadowFrustumCullPrimitives.reserve(ReserveHint);

		const std::shared_ptr<CameraComponent> MainCamera = View.GetMainCamera();
		if (!MainCamera)
			return;

		for (const auto& ActorItem : Actors)
		{
			if (ActorItem->GetState() != Actor::EActive || !ActorItem->IsVisible())
				continue;

			auto Components = std::move(ActorItem->GetComponents<GltfMeshComponent>());
			for (auto& ComponentItem : Components)
			{
				GltfSceneMeshInfo SceneMeshInfo;
				if (!ComponentItem->GatherMesh(SceneMeshInfo, MainCamera))
					continue;

				OutResult.ShadowFrustumCullPrimitives.push_back(SceneMeshInfo);
				if (ActorItem->IsProjectShadow())
					OutResult.DynamicShadowCastingPrimitives.push_back(SceneMeshInfo);

				OutResult.VisiblePrimitives.push_back(std::move(SceneMeshInfo));
			}
		}
	}
}
