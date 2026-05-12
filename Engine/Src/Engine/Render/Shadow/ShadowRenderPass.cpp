#include "Render/Shadow/ShadowRenderPass.h"
#include <algorithm>
#include <memory>
#include "core/vec2.h"
#include "Render/Shadow/FDirectionalShadowFrustumFitter.h"
#include "Render/Shadow/FDirectionalShadowDepthPass.h"
#include "Render/Shadow/FPointShadowCubePass.h"
#include "Render/Shadow/FShadowDepthMeshDrawer.h"
#include "Render/Shadow/FShadowViewData.h"
#include "Render/Shadow/FSpotShadowDepthPass.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIRenderTarget.h"
#include "RHI/RHITextureCube.h"

namespace Engine
{
	struct ShadowRenderPassPrivate
	{
		RenderCore::DynamicRHI* RHI;
		std::unique_ptr<FShadowDepthMeshDrawer> MeshDrawer;
		std::shared_ptr<RenderCore::RHIRenderTarget> DepthRenderBuffer;
		std::shared_ptr<RenderCore::RHIRenderTarget> SpotShadowBuffer;
		std::shared_ptr<RenderCore::RHITextureCube> PointShadowCube;
		Light CachedMainLightForShading{};
		bool bCachedMainLightValid = false;
		int CachedMainDirectionalShadowLightListIndex = -1;

		math::Matrix4x4 CachedPointFaceVP[6]{};
		math::Vector3 CachedPointLightPos{};
		float CachedPointLightRange = 0.f;
		int CachedPointShadowLightIndex = -1;
		bool bCachedPointShadowValid = false;

		math::Matrix4x4 CachedSpotLightViewProj{};
		math::Matrix4x4 CachedSpotLightView{};
		int CachedSpotShadowLightIndex = -1;
		bool bCachedSpotShadowValid = false;

		CBDirectionalShadow CachedDirectionalShadow{};

		explicit ShadowRenderPassPrivate(RenderCore::DynamicRHI* InRHI)
			: RHI(InRHI)
			, MeshDrawer(std::make_unique<FShadowDepthMeshDrawer>(InRHI))
		{
		}
	};

	ShadowRenderPass::ShadowRenderPass(RenderCore::DynamicRHI* RHI)
		: d_ptr(new ShadowRenderPassPrivate(RHI))
	{
	}

	ShadowRenderPass::~ShadowRenderPass()
	{
		delete d_ptr;
	}

	void ShadowRenderPass::InitResource()
	{
		C_P(ShadowRenderPass);
		const int32_t SHADOW_WIDTH = FDirectionalShadowDepthPass::kDirectionalShadowMapResolution;
		const int32_t SHADOW_HEIGHT = FDirectionalShadowDepthPass::kDirectionalShadowMapResolution;
		// D32 depth + comparison sampling (hardware PCF) for directional map; no R32 color target.
		d->DepthRenderBuffer = d->RHI->RHICreateRenderTarget(RenderCore::EPixelFormat::PF_ShadowDepth, SHADOW_WIDTH, SHADOW_HEIGHT, 1, false, false);
		if (!d->PointShadowCube)
			d->PointShadowCube = d->RHI->RHICreateTextureCube(RenderCore::EPixelFormat::PF_ShadowDepth, FPointShadowCubePass::kCubeFaceResolution,
															 FPointShadowCubePass::kCubeFaceResolution, 1, false);
		if (!d->SpotShadowBuffer)
			d->SpotShadowBuffer =
				d->RHI->RHICreateRenderTarget(RenderCore::EPixelFormat::PF_ShadowDepth, FSpotShadowDepthPass::kSpotShadowTextureSize,
											  FSpotShadowDepthPass::kSpotShadowTextureSize, 1, false, false);
	}

	void ShadowRenderPass::InvalidateCachedMainLightForShading()
	{
		C_P(ShadowRenderPass);
		d->bCachedMainLightValid = false;
		d->CachedMainDirectionalShadowLightListIndex = -1;
		d->CachedDirectionalShadow = CBDirectionalShadow{};
		d->bCachedPointShadowValid = false;
		d->bCachedSpotShadowValid = false;
	}

	bool ShadowRenderPass::TryGetCachedMainLightForShading(Light& OutLight, int* OutLightListIndexInLastShadowPassLights)
	{
		C_P(ShadowRenderPass);
		if (!d->bCachedMainLightValid)
			return false;
		OutLight = d->CachedMainLightForShading;
		if (OutLightListIndexInLastShadowPassLights)
			*OutLightListIndexInLastShadowPassLights = d->CachedMainDirectionalShadowLightListIndex;
		return true;
	}

	const CBDirectionalShadow& ShadowRenderPass::GetCachedDirectionalShadow() const
	{
		C_P(const ShadowRenderPass);
		return d->CachedDirectionalShadow;
	}

	void ShadowRenderPass::ClearCachedMeshShadowPasses()
	{
		C_P(ShadowRenderPass);
		if (d->MeshDrawer)
			d->MeshDrawer->ClearCache();
	}

	void ShadowRenderPass::Render(const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const std::vector<GltfSceneMeshInfo>& FrustumBoundsMeshes,
								  RenderCore::RHICommandContext& RHIContext, std::vector<Light> Lights, const FShadowProjectorSceneData& ShadowProjectorScene)
	{
		C_P(ShadowRenderPass);
		d->bCachedMainLightValid = false;
		d->CachedMainDirectionalShadowLightListIndex = -1;
		d->CachedDirectionalShadow = CBDirectionalShadow{};
		d->bCachedPointShadowValid = false;
		d->bCachedSpotShadowValid = false;

		if (ShadowCasterMeshes.empty() && FrustumBoundsMeshes.empty() && !ShadowProjectorScene.bValid)
			return;

		if (d->MeshDrawer)
			d->MeshDrawer->PruneStaleMeshShadowPasses(ShadowCasterMeshes, FrustumBoundsMeshes);

		FShadowViewData viewData = FShadowViewData::Build(ShadowCasterMeshes, FrustumBoundsMeshes, Lights, ShadowProjectorScene);

		if (viewData.LightSlots.DirectionalLightListIndex >= 0 && viewData.bSubjectValid && d->MeshDrawer)
		{
			const FShadowProjectorSceneData& projForAtlas = viewData.ProjectorScene;
			int atlasLayers = 1;
			if (projForAtlas.bDirectionalShadowCSM)
				atlasLayers = (std::clamp)(projForAtlas.DirectionalShadowCSMCascadeCount, 2, FDirectionalShadowFrustumFitter::kMaxDirectionalCascades);
			const int needW = FDirectionalShadowDepthPass::kDirectionalShadowMapResolution;
			const int needH = FDirectionalShadowDepthPass::kDirectionalShadowMapResolution * atlasLayers;
			if (!d->DepthRenderBuffer || d->DepthRenderBuffer->GetSize().x != needW || d->DepthRenderBuffer->GetSize().y != needH)
			{
				d->DepthRenderBuffer = d->RHI->RHICreateRenderTarget(RenderCore::EPixelFormat::PF_ShadowDepth, needW, needH, 1, false, false);
				if (d->MeshDrawer)
					d->MeshDrawer->ClearCache();
			}

			FDirectionalShadowDepthPassOutputs dirOut{};
			FDirectionalShadowDepthPassParameters dirParams{};
			dirParams.RHICmdList = &RHIContext;
			dirParams.ShadowCasterMeshes = viewData.ShadowCasterMeshes;
			dirParams.FrameLights = viewData.FrameLights;
			dirParams.MainDirectionalLightListIndex = viewData.LightSlots.DirectionalLightListIndex;
			dirParams.bSubjectValid = viewData.bSubjectValid;
			dirParams.SubjectWorldAabb = viewData.SubjectWorldAabb;
			dirParams.bReceiverValid = viewData.bReceiverValid;
			dirParams.ReceiverWorldAabb = viewData.ReceiverWorldAabb;
			dirParams.ProjectorScene = &viewData.ProjectorScene;
			dirParams.SubjectMeshListForFrustumDriver = viewData.SubjectMeshListForFrustum;
			dirParams.DepthRenderBuffer = d->DepthRenderBuffer;
			dirParams.MeshDrawer = d->MeshDrawer.get();
			dirParams.OutOutputs = &dirOut;
			FDirectionalShadowDepthPass::Render(dirParams);
			d->CachedMainLightForShading = dirOut.CachedMainLightForShading;
			d->bCachedMainLightValid = dirOut.bCachedMainLightValid;
			d->CachedMainDirectionalShadowLightListIndex = dirOut.CachedMainDirectionalShadowLightListIndex;
			d->CachedDirectionalShadow = dirOut.CachedDirectionalShadow;
		}

		if (viewData.LightSlots.PointCubeShadowLightListIndex >= 0 && d->PointShadowCube && viewData.ShadowCasterMeshes && !viewData.ShadowCasterMeshes->empty()
			&& d->MeshDrawer)
		{
			FPointShadowCubePass::FOutputs ptOut{};
			const int ptIdx = viewData.LightSlots.PointCubeShadowLightListIndex;
			FPointShadowCubePass::FPointShadowCubePassParameters ptParams{};
			ptParams.RHICmdList = &RHIContext;
			ptParams.ShadowCasterMeshes = viewData.ShadowCasterMeshes;
			ptParams.PointLight = &Lights[static_cast<size_t>(ptIdx)];
			ptParams.PointLightListIndex = ptIdx;
			ptParams.PointShadowCube = d->PointShadowCube;
			ptParams.MeshDrawer = d->MeshDrawer.get();
			ptParams.OutOutputs = &ptOut;
			FPointShadowCubePass::Render(ptParams);
			for (int i = 0; i < 6; ++i)
				d->CachedPointFaceVP[i] = ptOut.CachedPointFaceVP[i];
			d->CachedPointLightPos = ptOut.CachedPointLightPos;
			d->CachedPointLightRange = ptOut.CachedPointLightRange;
			d->CachedPointShadowLightIndex = ptOut.CachedPointShadowLightIndex;
			d->bCachedPointShadowValid = ptOut.bCachedPointShadowValid;
		}

		if (viewData.LightSlots.SpotShadowLightListIndex >= 0 && d->SpotShadowBuffer && d->MeshDrawer)
		{
			FSpotShadowDepthPass::FOutputs spotOut{};
			FSpotShadowDepthPass::FSpotShadowDepthPassParameters spParams{};
			spParams.RHICmdList = &RHIContext;
			spParams.ShadowCasterMeshes = viewData.ShadowCasterMeshes;
			spParams.FrustumBoundsMeshes = viewData.FrustumBoundsMeshes;
			spParams.FrameLights = viewData.FrameLights;
			spParams.SpotLightListIndex = viewData.LightSlots.SpotShadowLightListIndex;
			spParams.bSubjectValid = viewData.bSubjectValid;
			spParams.SubjectWorldAabb = viewData.SubjectWorldAabb;
			spParams.bReceiverValid = viewData.bReceiverValid;
			spParams.ReceiverWorldAabb = viewData.ReceiverWorldAabb;
			spParams.SpotShadowBuffer = d->SpotShadowBuffer;
			spParams.MeshDrawer = d->MeshDrawer.get();
			spParams.OutOutputs = &spotOut;
			FSpotShadowDepthPass::Render(spParams);
			d->CachedSpotLightViewProj = spotOut.CachedSpotLightViewProj;
			d->CachedSpotLightView = spotOut.CachedSpotLightView;
			d->CachedSpotShadowLightIndex = spotOut.CachedSpotShadowLightIndex;
			d->bCachedSpotShadowValid = spotOut.bCachedSpotShadowValid;
		}

		if (d->DepthRenderBuffer)
		{
			const core::vec2i ds = d->DepthRenderBuffer->GetSize();
			RHIContext.SetViewPort(0, 0, ds.x, ds.y);
		}
	}

	std::shared_ptr<RenderCore::RHIRenderTarget> ShadowRenderPass::GetShadowMap() const
	{
		C_P(const ShadowRenderPass);
		return d->DepthRenderBuffer;
	}

	std::shared_ptr<RenderCore::RHITextureCube> ShadowRenderPass::GetPointShadowCube() const
	{
		C_P(const ShadowRenderPass);
		return d->PointShadowCube;
	}

	bool ShadowRenderPass::TryGetCachedPointShadowForDeferred(int& OutLightIndex, math::Matrix4x4 OutFaceVp[6], math::Vector4& OutPosRange) const
	{
		C_P(const ShadowRenderPass);
		if (!d->bCachedPointShadowValid)
			return false;
		for (int i = 0; i < 6; ++i)
			OutFaceVp[i] = d->CachedPointFaceVP[i];
		OutLightIndex = d->CachedPointShadowLightIndex;
		OutPosRange = math::Vector4(d->CachedPointLightPos.x, d->CachedPointLightPos.y, d->CachedPointLightPos.z, d->CachedPointLightRange);
		return true;
	}

	std::shared_ptr<RenderCore::RHIRenderTarget> ShadowRenderPass::GetSpotShadowMap() const
	{
		C_P(const ShadowRenderPass);
		return d->SpotShadowBuffer;
	}

	bool ShadowRenderPass::TryGetCachedSpotShadowForDeferred(int& OutLightIndex, math::Matrix4x4& OutSpotLightViewProj, math::Matrix4x4* OutOptionalLightView) const
	{
		C_P(const ShadowRenderPass);
		if (!d->bCachedSpotShadowValid)
			return false;
		OutLightIndex = d->CachedSpotShadowLightIndex;
		OutSpotLightViewProj = d->CachedSpotLightViewProj;
		if (OutOptionalLightView)
			*OutOptionalLightView = d->CachedSpotLightView;
		return true;
	}

} // namespace Engine
