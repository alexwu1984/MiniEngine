#include "Render/SceneRendering/SceneRendererPrimitiveGather.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "Scene/FScene.h"
#include "Material/MaterialBase.h"
#include "GltfModel/GltfMesh.h"

namespace Engine
{
	bool FSceneRendererPrimitiveGather::SceneContainsTransmissionMesh(const std::vector<GltfSceneMeshInfo>& SceneMeshInfos)
	{
		for (const auto& Info : SceneMeshInfos)
		{
			for (const auto& Mesh : Info.Meshes)
			{
				const std::shared_ptr<MaterialBase> meshMat = Mesh ? Mesh->GetMaterial() : nullptr;
				if (meshMat && meshMat->UsesTransmissionShading())
					return true;
			}
		}
		return false;
	}

	void FSceneRendererPrimitiveGather::GatherVisiblePrimitives(const FSceneViewData& ViewData, const FScene& Scene,
																FPrimitiveGatherResult& OutResult)
	{
		OutResult.VisiblePrimitives.clear();
		OutResult.DynamicShadowCastingPrimitives.clear();
		OutResult.ShadowFrustumCullPrimitives.clear();

		const std::vector<std::shared_ptr<FPrimitiveSceneProxy>> Proxies = Scene.SnapshotPrimitives();
		const std::size_t ProxyCount = Proxies.size();
		const std::size_t ReserveHint = ProxyCount * 4u + 8u;
		OutResult.VisiblePrimitives.reserve(ReserveHint);
		OutResult.DynamicShadowCastingPrimitives.reserve(ProxyCount + 4u);
		OutResult.ShadowFrustumCullPrimitives.reserve(ReserveHint);

		for (const auto& Proxy : Proxies)
		{
			if (!Proxy)
				continue;
			Proxy->AppendForView(ViewData, OutResult);
		}
	}
}
