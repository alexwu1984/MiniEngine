#include "Render/Shadow/FDirectionalShadowDepthPass.h"
#include "Render/Shadow/FDirectionalShadowFrustumFitter.h"
#include "Render/Shadow/FShadowDepthMeshDrawer.h"
#include "Render/Shadow/FShadowSceneBounds.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIRenderTarget.h"
#include "core/color.h"

namespace Engine
{
	static void RenderDirectionalShadowMapFullViewport(RenderCore::RHICommandContext& RHIContext, const std::shared_ptr<RenderCore::RHIRenderTarget>& DepthRenderBuffer,
													 FShadowDepthMeshDrawer& MeshDrawer, const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const Light& MainLight)
	{
		RHIContext.Clear(DepthRenderBuffer, core::FLinearColor::White, 1.f, 0);
		const auto TargetSize = DepthRenderBuffer->GetSize();
		RHIContext.SetViewPort(0, 0, TargetSize.x, TargetSize.y);
		MeshDrawer.DrawDirectional(RHIContext, ShadowCasterMeshes, MainLight, DepthRenderBuffer);
	}

	void FDirectionalShadowDepthPass::Render(RenderCore::RHICommandContext& RHIContext, const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, std::vector<Light>& Lights,
											 int MainDirLightIndex, bool bSubjectValid, const math::AABB3& SubjectWorldAabb, bool bReceiverValid,
											 const math::AABB3& ReceiverWorldAabb, const FShadowProjectorSceneData& ShadowProjectorScene,
											 const std::vector<GltfSceneMeshInfo>* SubjectMeshListForFrustumDriver, const std::shared_ptr<RenderCore::RHIRenderTarget>& DepthRenderBuffer,
											 FShadowDepthMeshDrawer& MeshDrawer, FDirectionalShadowDepthPassOutputs& OutOutputs)
	{
		OutOutputs.bCachedMainLightValid = false;
		OutOutputs.CachedMainDirectionalShadowLightListIndex = -1;
		OutOutputs.bCachedDirectionalCSMParamsValid = false;
		OutOutputs.CachedDirectionalCSM = CBDirectionalShadowCSM{};

		if (MainDirLightIndex < 0 || !bSubjectValid || !DepthRenderBuffer)
			return;

		Light& mainLightRef = Lights[static_cast<size_t>(MainDirLightIndex)];
		mainLightRef.ShadowMapIndex = 0;
		const core::vec2i fullTexSize = DepthRenderBuffer->GetSize();
		const core::vec2i cascadeTexSize{ kCascadeShadowResolution, kCascadeShadowResolution };
		const bool bReceiverRelativeFrustumAdjust =
			bReceiverValid && FShadowSceneBounds::kPreferTightShadowFrustumFromCasters && SubjectMeshListForFrustumDriver == &ShadowCasterMeshes;

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

			RHIContext.Clear(DepthRenderBuffer, core::FLinearColor::White, 1.f, 0);

			const float camNear = ShadowProjectorScene.CameraNearZ;
			Light firstCascadeLight{};
			for (int ci = 0; ci < FDirectionalShadowFrustumFitter::kCascadeCount; ++ci)
			{
				const float zNearSlice = (ci == 0) ? camNear : splitEnds[ci - 1];
				const float zFarSlice = splitEnds[ci];
				const math::AABB3 sliceBounds = FDirectionalShadowFrustumFitter::WorldBoundsFromViewProjSliceInverse(
					ShadowProjectorScene.CameraView, ShadowProjectorScene.CameraFovYRad, ShadowProjectorScene.CameraAspectWH, zNearSlice, zFarSlice);
				const math::AABB3 cascadeSubject = SubjectWorldAabb.MergeAABB(sliceBounds);

				Light Li = mainLightRef;
				FDirectionalShadowFrustumFitter::SetupDirectionalShadowViewProjection(Li, cascadeSubject, bReceiverRelativeFrustumAdjust, ReceiverWorldAabb, cascadeTexSize,
																					ShadowProjectorScene, false);
				OutOutputs.CachedDirectionalCSM.CascadeViewProj[ci] = Li.LightViewProj;
				if (ci == 0)
					firstCascadeLight = Li;

				RHIContext.SetViewPort(0, ci * kCascadeShadowResolution, kCascadeShadowResolution, kCascadeShadowResolution);
				MeshDrawer.DrawDirectional(RHIContext, ShadowCasterMeshes, Li, DepthRenderBuffer);
			}
			OutOutputs.CachedMainLightForShading = firstCascadeLight;
			OutOutputs.CachedMainLightForShading.ShadowMapIndex = 0;
			OutOutputs.CachedMainDirectionalShadowLightListIndex = MainDirLightIndex;
			OutOutputs.bCachedMainLightValid = true;
		}
		else
		{
			FDirectionalShadowFrustumFitter::SetupDirectionalShadowViewProjection(mainLightRef, SubjectWorldAabb, bReceiverRelativeFrustumAdjust, ReceiverWorldAabb, fullTexSize,
																					ShadowProjectorScene, true);
			OutOutputs.CachedMainLightForShading = mainLightRef;
			OutOutputs.CachedMainDirectionalShadowLightListIndex = MainDirLightIndex;
			OutOutputs.bCachedMainLightValid = true;
			RHIContext.SetViewPort(0, 0, fullTexSize.x, fullTexSize.y);
			RenderDirectionalShadowMapFullViewport(RHIContext, DepthRenderBuffer, MeshDrawer, ShadowCasterMeshes, mainLightRef);
		}
	}
}
