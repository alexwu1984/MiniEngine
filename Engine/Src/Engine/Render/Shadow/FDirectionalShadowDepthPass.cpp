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
		OutOutputs.CachedDirectionalShadow = CBDirectionalShadow{};

		if (P.MainDirectionalLightListIndex < 0 || !P.bSubjectValid || !P.DepthRenderBuffer)
			return;

		RenderCore::RHICommandContext& RHIContext = *P.RHICmdList;
		std::vector<Light>& Lights = *P.FrameLights;
		const FShadowProjectorSceneData& ShadowProjectorScene = *P.ProjectorScene;
		const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes = *P.ShadowCasterMeshes;
		FShadowDepthMeshDrawer& MeshDrawer = *P.MeshDrawer;

		Light& mainLightRef = Lights[static_cast<size_t>(P.MainDirectionalLightListIndex)];
		mainLightRef.ShadowMapIndex = 0;
		const core::vec2i shadowTexSize{ kDirectionalShadowMapResolution, kDirectionalShadowMapResolution };
		const bool bReceiverRelativeFrustumAdjust =
			P.bReceiverValid && FShadowSceneBounds::kPreferTightShadowFrustumFromCasters && P.SubjectMeshListForFrustumDriver == &ShadowCasterMeshes;

		Light Li = mainLightRef;
		FDirectionalShadowFrustumFitter::SetupDirectionalShadowViewProjection(Li, P.SubjectWorldAabb, bReceiverRelativeFrustumAdjust, P.ReceiverWorldAabb, shadowTexSize,
																				ShadowProjectorScene, false, P.SubjectMeshListForFrustumDriver, nullptr);
		OutOutputs.CachedDirectionalShadow.ViewProj = Li.LightViewProj;
		OutOutputs.CachedMainLightForShading = Li;
		OutOutputs.CachedMainLightForShading.ShadowMapIndex = 0;
		OutOutputs.CachedMainDirectionalShadowLightListIndex = P.MainDirectionalLightListIndex;
		OutOutputs.bCachedMainLightValid = true;

		RHIContext.Clear(P.DepthRenderBuffer, core::FLinearColor::White, 1.f, 0);
		RHIContext.SetViewPort(0, 0, kDirectionalShadowMapResolution, kDirectionalShadowMapResolution);
		MeshDrawer.DrawDirectional(RHIContext, ShadowCasterMeshes, Li, P.DepthRenderBuffer);
	}
}
