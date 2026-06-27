#include "Render/Shadow/ShadowViewData.h"
#include "Render/Shadow/DirectionalShadowDepthPass.h"
#include "Render/Shadow/PointShadowCubePass.h"
#include "Render/Shadow/ShadowSceneBounds.h"
#include "Render/Shadow/SpotShadowDepthPass.h"
#include "Scene/SceneMeshComponent.h"

namespace Engine
{
	FShadowViewData FShadowViewData::Build(const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const std::vector<GltfSceneMeshInfo>& FrustumBoundsMeshes,
										   std::vector<Light>& Lights, const FShadowProjectorSceneData& ShadowProjectorScene)
	{
		FShadowViewData Out{};
		Out.ShadowCasterMeshes = &ShadowCasterMeshes;
		Out.FrustumBoundsMeshes = &FrustumBoundsMeshes;
		Out.FrameLights = &Lights;
		Out.ProjectorScene = ShadowProjectorScene;
		Out.SubjectMeshListForFrustum = FShadowSceneBounds::SelectShadowSubjectMeshListForFrustum(ShadowCasterMeshes, FrustumBoundsMeshes, ShadowProjectorScene);

		Out.LightSlots.DirectionalLightListIndex = FDirectionalShadowDepthPass::FindFirstDirectionalLightIndex(Lights);
		Out.LightSlots.PointCubeShadowLightListIndex = FPointShadowCubePass::FindPointShadowCubeLightIndex(Lights);
		Out.LightSlots.SpotShadowLightListIndex = FSpotShadowDepthPass::FindSpotShadowLightIndex(Lights);

		FShadowSceneBounds::BuildMergedShadowSubjectWorldAabb(Out.SubjectMeshListForFrustum, ShadowProjectorScene, Out.SubjectWorldAabb, Out.bSubjectValid);
		FShadowSceneBounds::BuildMergedShadowReceiverWorldAabb(FrustumBoundsMeshes, Out.ReceiverWorldAabb, Out.bReceiverValid);

		return Out;
	}
}
