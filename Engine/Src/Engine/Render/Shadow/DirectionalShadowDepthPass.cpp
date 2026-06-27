#include "Render/Shadow/DirectionalShadowDepthPass.h"
#include "Render/Shadow/DirectionalShadowFrustumFitter.h"
#include "Render/Shadow/ShadowDepthMeshDrawer.h"
#include "Render/Shadow/ShadowSceneBounds.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIRenderTarget.h"
#include "core/color.h"

namespace
{
	/** Shared context built once in `Render` after validation; non-CSM / CSM paths stay linear without interleaved mode checks. */
	struct FDirShadowDepthRenderCommon
	{
		Engine::FDirectionalShadowDepthPassOutputs& OutOutputs;
		RenderCore::RHICommandContext& RHIContext;
		Engine::Light& MainLightRef;
		const std::vector<Engine::GltfSceneMeshInfo>& ShadowCasterMeshes;
		Engine::FShadowDepthMeshDrawer& MeshDrawer;
		const Engine::FShadowProjectorSceneData& ProjectorScene;
		const Engine::FDirectionalShadowDepthPassParameters& Pass;
		const int TilePx;
		const core::vec2i ShadowTexSize;
		const bool bReceiverRelativeFrustumAdjust;
		math::Vector3 CameraForwardNorm{};
	};

	static void RenderDirectionalShadowNonCSM(const FDirShadowDepthRenderCommon& C)
	{
		using namespace Engine;
		Light Li = C.MainLightRef;
		const FDirectionalShadowFrustumFitParams fitParams{
			C.Pass.SubjectWorldAabb,
			C.Pass.ReceiverWorldAabb,
			C.ShadowTexSize,
			C.ProjectorScene,
			C.bReceiverRelativeFrustumAdjust,
			false,
			C.Pass.SubjectMeshListForFrustumDriver,
			nullptr,
		};
		FDirectionalShadowFrustumFitter::SetupDirectionalShadowViewProjection(Li, fitParams);

		CBDirectionalShadow& cb = C.OutOutputs.CachedDirectionalShadow;
		cb.CascadeViewProj[0] = Li.LightViewProj;
		cb.CascadeSplits = math::Vector4(0.f, 0.f, 0.f, 0.f);
		cb.CameraForwardInvCount = math::Vector4(C.CameraForwardNorm.x, C.CameraForwardNorm.y, C.CameraForwardNorm.z, 1.f);
		cb.DirectionalCSMEnabled = 0;
		cb.CascadeCount = 1;

		C.OutOutputs.CachedMainLightForShading = Li;
		C.OutOutputs.CachedMainLightForShading.ShadowMapIndex = 0;
		C.OutOutputs.CachedMainDirectionalShadowLightListIndex = C.Pass.MainDirectionalLightListIndex;
		C.OutOutputs.bCachedMainLightValid = true;

		C.RHIContext.Clear(C.Pass.DepthRenderBuffer, core::FLinearColor::White, 1.f, 0);
		C.RHIContext.SetViewPort(0, 0, C.TilePx, C.TilePx);
		C.MeshDrawer.DrawDirectional(C.RHIContext, C.ShadowCasterMeshes, Li, C.Pass.DepthRenderBuffer);
	}

	static void RenderDirectionalShadowCSM(const FDirShadowDepthRenderCommon& C)
	{
		using namespace Engine;
		const int cascadeCount =
			(std::clamp)(C.ProjectorScene.DirectionalShadowCSMCascadeCount, 2, FDirectionalShadowFrustumFitter::kMaxDirectionalCascades);
		float splitZE[2]{};
		FDirectionalShadowFrustumFitter::FillLinearZeSplitEnds(C.ProjectorScene.CameraNearZ, C.ProjectorScene.CameraFarZ, cascadeCount,
															  C.ProjectorScene.DirectionalShadowCSMSplit0, C.ProjectorScene.DirectionalShadowCSMSplit1, splitZE);

		CBDirectionalShadow& cb = C.OutOutputs.CachedDirectionalShadow;
		cb.DirectionalCSMEnabled = 1;
		cb.CascadeCount = cascadeCount;
		cb.CascadeSplits = math::Vector4(splitZE[0], splitZE[1], 0.f, 0.f);
		cb.CameraForwardInvCount =
			math::Vector4(C.CameraForwardNorm.x, C.CameraForwardNorm.y, C.CameraForwardNorm.z, 1.f / static_cast<float>((std::max)(cascadeCount, 1)));

		C.RHIContext.Clear(C.Pass.DepthRenderBuffer, core::FLinearColor::White, 1.f, 0);

		Light firstCascadeLight{};
		for (int ci = 0; ci < cascadeCount; ++ci)
		{
			const FCascadeSubjectWorldAabbParams cascadeSubjectParams{
				C.ProjectorScene,
				C.Pass.SubjectWorldAabb,
				C.Pass.bReceiverValid ? &C.Pass.ReceiverWorldAabb : nullptr,
				ci,
				{ splitZE[0], splitZE[1] },
				cascadeCount,
			};
			const math::AABB3 cascadeSubject = FDirectionalShadowFrustumFitter::ComputeCascadeSubjectWorldAabb(cascadeSubjectParams);

			Light Li = C.MainLightRef;
			// CSM must clip mesh extents to this cascade's world subject; otherwise TryMergeSubjectMeshesLightSpaceExtents
			// fits the full caster every time and all tiles get identical ortho + depth.
			const FDirectionalShadowFrustumFitParams fitParams{
				cascadeSubject,
				C.Pass.ReceiverWorldAabb,
				C.ShadowTexSize,
				C.ProjectorScene,
				C.bReceiverRelativeFrustumAdjust,
				false,
				C.Pass.SubjectMeshListForFrustumDriver,
				&cascadeSubject,
			};
			FDirectionalShadowFrustumFitter::SetupDirectionalShadowViewProjection(Li, fitParams);
			cb.CascadeViewProj[ci] = Li.LightViewProj;
			if (ci < FDirectionalShadowDepthPassOutputs::kMaxCascadeSubjectDebug)
				C.OutOutputs.CascadeSubjectWorldAabbDebug[ci] = cascadeSubject;
			if (ci == 0)
				firstCascadeLight = Li;

			const int vpY = ci * C.TilePx;
			C.RHIContext.SetViewPort(0, vpY, C.TilePx, C.TilePx);
			C.MeshDrawer.DrawDirectional(C.RHIContext, C.ShadowCasterMeshes, Li, C.Pass.DepthRenderBuffer);
		}
		C.OutOutputs.CascadeSubjectAabbDebugCount = cascadeCount;

		C.OutOutputs.CachedMainLightForShading = firstCascadeLight;
		C.OutOutputs.CachedMainLightForShading.ShadowMapIndex = 0;
		C.OutOutputs.CachedMainDirectionalShadowLightListIndex = C.Pass.MainDirectionalLightListIndex;
		C.OutOutputs.bCachedMainLightValid = true;
	}
} // namespace

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
		OutOutputs.CachedDirectionalShadow = CBDirectionalShadow{};
		OutOutputs.CascadeSubjectAabbDebugCount = 0;
		for (int i = 0; i < FDirectionalShadowDepthPassOutputs::kMaxCascadeSubjectDebug; ++i)
			OutOutputs.CascadeSubjectWorldAabbDebug[i] = math::AABB3{};

		if (P.MainDirectionalLightListIndex < 0 || !P.bSubjectValid || !P.DepthRenderBuffer)
			return;

		RenderCore::RHICommandContext& RHIContext = *P.RHICmdList;
		std::vector<Light>& Lights = *P.FrameLights;
		const FShadowProjectorSceneData& ShadowProjectorScene = *P.ProjectorScene;
		const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes = *P.ShadowCasterMeshes;
		FShadowDepthMeshDrawer& MeshDrawer = *P.MeshDrawer;

		Light& mainLightRef = Lights[static_cast<size_t>(P.MainDirectionalLightListIndex)];
		mainLightRef.ShadowMapIndex = 0;
		const int tilePx = kDirectionalShadowMapResolution;
		const core::vec2i shadowTexSize{ tilePx, tilePx };
		const bool bReceiverRelativeFrustumAdjust =
			P.bReceiverValid && FShadowSceneBounds::kPreferTightShadowFrustumFromCasters && P.SubjectMeshListForFrustumDriver == &ShadowCasterMeshes;

		math::Vector3 cameraForwardNorm = ShadowProjectorScene.CameraForwardWorld;
		if (cameraForwardNorm.GetSqrLength() > 1e-12f)
			cameraForwardNorm = cameraForwardNorm.Normalize();
		else
			cameraForwardNorm = math::Vector3(0.f, 0.f, 1.f);

		const FDirShadowDepthRenderCommon common{
			OutOutputs,
			RHIContext,
			mainLightRef,
			ShadowCasterMeshes,
			MeshDrawer,
			ShadowProjectorScene,
			P,
			tilePx,
			shadowTexSize,
			bReceiverRelativeFrustumAdjust,
			cameraForwardNorm,
		};

		if (ShadowProjectorScene.bDirectionalShadowCSM)
		{
			RenderDirectionalShadowCSM(common);
		}
		else
		{
			RenderDirectionalShadowNonCSM(common);
		}
		
	}
}
