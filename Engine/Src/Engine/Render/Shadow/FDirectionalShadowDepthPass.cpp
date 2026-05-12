#include "Render/Shadow/FDirectionalShadowDepthPass.h"
#include "Render/Shadow/FDirectionalShadowFrustumFitter.h"
#include "Render/Shadow/FShadowDepthMeshDrawer.h"
#include "Render/Shadow/FShadowSceneBounds.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIRenderTarget.h"
#include "core/color.h"

namespace Engine
{
	int FDirectionalShadowDepthPass::FindFirstDirectionalLightIndex(const std::vector<Light>& Lights)
	{
		for (int i = 0; i < static_cast<int>(Lights.size()); ++i)
		{
			if (Lights[static_cast<size_t>(i)].Type == LightType_Directional)
				return i;
		}
		return -1;
	}

	void FDirectionalShadowDepthPass::Render(const FDirectionalShadowDepthPassParameters& P)
	{
		if (!P.OutOutputs || !P.RHICmdList || !P.ShadowCasterMeshes || !P.FrameLights || !P.ProjectorScene || !P.MeshDrawer)
			return;

		FDirectionalShadowDepthPassOutputs& OutOutputs = *P.OutOutputs;
		OutOutputs.bCachedMainLightValid = false;
		OutOutputs.CachedMainDirectionalShadowLightListIndex = -1;
		OutOutputs.CachedDirectionalCSM = CBDirectionalShadowCSM{};

		if (P.MainDirectionalLightListIndex < 0 || !P.bSubjectValid || !P.DepthRenderBuffer)
			return;

		RenderCore::RHICommandContext& RHIContext = *P.RHICmdList;
		std::vector<Light>& Lights = *P.FrameLights;
		const FShadowProjectorSceneData& ShadowProjectorScene = *P.ProjectorScene;
		const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes = *P.ShadowCasterMeshes;
		FShadowDepthMeshDrawer& MeshDrawer = *P.MeshDrawer;

		Light& mainLightRef = Lights[static_cast<size_t>(P.MainDirectionalLightListIndex)];
		mainLightRef.ShadowMapIndex = 0;
		const core::vec2i cascadeTexSize{ kCascadeShadowResolution, kCascadeShadowResolution };
		const bool bReceiverRelativeFrustumAdjust =
			P.bReceiverValid && FShadowSceneBounds::kPreferTightShadowFrustumFromCasters && P.SubjectMeshListForFrustumDriver == &ShadowCasterMeshes;

		OutOutputs.CachedDirectionalCSM = CBDirectionalShadowCSM{};
		OutOutputs.CachedDirectionalCSM.DirectionalCSMEnabled = 1;

		float splitEnds[FDirectionalShadowFrustumFitter::kCascadeCount];
		FDirectionalShadowFrustumFitter::ComputeDirectionalCascadeSplitEnds(ShadowProjectorScene.CameraNearZ, ShadowProjectorScene.CameraFarZ, splitEnds);
		OutOutputs.CachedDirectionalCSM.CascadeSplits =
			math::Vector4(splitEnds[0], splitEnds[1], ShadowProjectorScene.CameraFarZ, static_cast<float>(FDirectionalShadowFrustumFitter::kCascadeCount));
		const float invN = 1.f / static_cast<float>(FDirectionalShadowFrustumFitter::kCascadeCount);
		OutOutputs.CachedDirectionalCSM.CameraForwardInvCount =
			math::Vector4(ShadowProjectorScene.CameraForwardWorld.x, ShadowProjectorScene.CameraForwardWorld.y, ShadowProjectorScene.CameraForwardWorld.z, invN);

		RHIContext.Clear(P.DepthRenderBuffer, core::FLinearColor::White, 1.f, 0);

		Light firstCascadeLight{};
		for (int ci = 0; ci < FDirectionalShadowFrustumFitter::kCascadeCount; ++ci)
		{
			// Fit ortho to merged shadow casters only. Do NOT clip SubjectWorldAabb by the slice's *world AABB*:
			// that AABB is only a loose hull of the frustum wedge; intersecting can shrink below the real slice
			// and crop casters out of the shadow map (CSM0 shows a tiny silhouette). Merge(Subject, slice) was
			// equally wrong (union → huge waste). Per-cascade split is applied in deferred sampling, not here.
			const math::AABB3& cascadeSubject = P.SubjectWorldAabb;

			Light Li = mainLightRef;
			FDirectionalShadowFrustumFitter::SetupDirectionalShadowViewProjection(Li, cascadeSubject, bReceiverRelativeFrustumAdjust, P.ReceiverWorldAabb, cascadeTexSize,
																					ShadowProjectorScene, false, P.SubjectMeshListForFrustumDriver, nullptr);
			OutOutputs.CachedDirectionalCSM.CascadeViewProj[ci] = Li.LightViewProj;
			if (ci == 0)
				firstCascadeLight = Li;

			RHIContext.SetViewPort(0, ci * kCascadeShadowResolution, kCascadeShadowResolution, kCascadeShadowResolution);
			MeshDrawer.DrawDirectional(RHIContext, ShadowCasterMeshes, Li, P.DepthRenderBuffer);
		}
		OutOutputs.CachedMainLightForShading = firstCascadeLight;
		OutOutputs.CachedMainLightForShading.ShadowMapIndex = 0;
		OutOutputs.CachedMainDirectionalShadowLightListIndex = P.MainDirectionalLightListIndex;
		OutOutputs.bCachedMainLightValid = true;
	}
}
