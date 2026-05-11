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
		OutOutputs.bCachedDirectionalCSMParamsValid = false;
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

		OutOutputs.bCachedDirectionalCSMParamsValid = true;
		OutOutputs.CachedDirectionalCSM = CBDirectionalShadowCSM{};
		OutOutputs.CachedDirectionalCSM.DirectionalCSMEnabled = 0;

		if (ShadowProjectorScene.bHasCascadeCameraParams)
		{
			float splitEnds[FDirectionalShadowFrustumFitter::kCascadeCount];
			FDirectionalShadowFrustumFitter::ComputeDirectionalCascadeSplitEnds(ShadowProjectorScene.CameraNearZ, ShadowProjectorScene.CameraFarZ, splitEnds);
			OutOutputs.CachedDirectionalCSM.DirectionalCSMEnabled = 1;
			OutOutputs.CachedDirectionalCSM.CascadeSplits =
				math::Vector4(splitEnds[0], splitEnds[1], ShadowProjectorScene.CameraFarZ, static_cast<float>(FDirectionalShadowFrustumFitter::kCascadeCount));
			const float invN = 1.f / static_cast<float>(FDirectionalShadowFrustumFitter::kCascadeCount);
			OutOutputs.CachedDirectionalCSM.CameraForwardInvCount =
				math::Vector4(ShadowProjectorScene.CameraForwardWorld.x, ShadowProjectorScene.CameraForwardWorld.y, ShadowProjectorScene.CameraForwardWorld.z, invN);

			RHIContext.Clear(P.DepthRenderBuffer, core::FLinearColor::White, 1.f, 0);

			const float camNear = ShadowProjectorScene.CameraNearZ;
			Light firstCascadeLight{};
			for (int ci = 0; ci < FDirectionalShadowFrustumFitter::kCascadeCount; ++ci)
			{
				const float zNearSlice = (ci == 0) ? camNear : splitEnds[ci - 1];
				const float zFarSlice = splitEnds[ci];
				const math::AABB3 sliceBounds = FDirectionalShadowFrustumFitter::WorldBoundsFromViewProjSliceInverse(
					ShadowProjectorScene.CameraView, ShadowProjectorScene.CameraFovYRad, ShadowProjectorScene.CameraAspectWH, zNearSlice, zFarSlice);
				const math::AABB3 cascadeSubject = P.SubjectWorldAabb.MergeAABB(sliceBounds);

				Light Li = mainLightRef;
				FDirectionalShadowFrustumFitter::SetupDirectionalShadowViewProjection(Li, cascadeSubject, bReceiverRelativeFrustumAdjust, P.ReceiverWorldAabb, cascadeTexSize,
																					ShadowProjectorScene, false);
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
		else
		{
			// Match first CSM atlas tile (square 2048²): rendering to the full 2048×(2048·N) atlas with one ortho made UV
			// kernels anisotropic in texel space (3× taller) and axis-aligned PCSS + texel snap caused strong vertical banding.
			FDirectionalShadowFrustumFitter::SetupDirectionalShadowViewProjection(mainLightRef, P.SubjectWorldAabb, bReceiverRelativeFrustumAdjust, P.ReceiverWorldAabb, cascadeTexSize,
																					ShadowProjectorScene, true);
			const float invN = 1.f / static_cast<float>(FDirectionalShadowFrustumFitter::kCascadeCount);
			OutOutputs.CachedDirectionalCSM.CameraForwardInvCount.w = invN;
			OutOutputs.CachedMainLightForShading = mainLightRef;
			OutOutputs.CachedMainDirectionalShadowLightListIndex = P.MainDirectionalLightListIndex;
			OutOutputs.bCachedMainLightValid = true;
			RHIContext.Clear(P.DepthRenderBuffer, core::FLinearColor::White, 1.f, 0);
			RHIContext.SetViewPort(0, 0, cascadeTexSize.x, cascadeTexSize.y);
			MeshDrawer.DrawDirectional(RHIContext, ShadowCasterMeshes, mainLightRef, P.DepthRenderBuffer);
		}
	}
}
