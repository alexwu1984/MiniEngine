#include "Render/Shadow/FShadowViewData.h"
#include "Render/Shadow/FDirectionalShadowDepthPass.h"
#include "Render/Shadow/FPointShadowCubePass.h"
#include "Render/Shadow/FShadowSceneBounds.h"
#include "Render/Shadow/FSpotShadowDepthPass.h"
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
