#include "Render/Shadow/ShadowRenderPass.h"
#include <memory>
#include "core/vec2.h"
#include "Render/Shadow/FDirectionalShadowDepthPass.h"
#include "Render/Shadow/FDirectionalShadowFrustumFitter.h"
#include "Render/Shadow/FPointShadowCubePass.h"
#include "Render/Shadow/FShadowDepthMeshDrawer.h"
#include "Render/Shadow/FShadowSceneBounds.h"
#include "Render/Shadow/FSpotShadowDepthPass.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIRenderTarget.h"
#include "RHI/RHITextureCube.h"

namespace Engine
{
	static int FindFirstDirectionalLightIndex(const std::vector<Light>& Lights)
	{
		for (int i = 0; i < static_cast<int>(Lights.size()); ++i)
		{
			if (Lights[static_cast<size_t>(i)].Type == LightType_Directional)
				return i;
		}
		return -1;
	}

	static int FindPointShadowCubeLightIndex(const std::vector<Light>& Lights)
	{
		for (int i = 0; i < static_cast<int>(Lights.size()); ++i)
		{
			const Light& L = Lights[static_cast<size_t>(i)];
			if (L.Type == LightType_Point && L.ShadowMapIndex == kPointLightCubeShadowMapIndex)
				return i;
		}
		return -1;
	}

	static int FindSpotShadowLightIndex(const std::vector<Light>& Lights)
	{
		for (int i = 0; i < static_cast<int>(Lights.size()); ++i)
		{
			const Light& L = Lights[static_cast<size_t>(i)];
			if (L.Type == LightType_Spot && L.ShadowMapIndex == kSpotLightShadowMapIndex)
				return i;
		}
		return -1;
	}

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

		CBDirectionalShadowCSM CachedDirectionalCSM{};
		bool bCachedDirectionalCSMParamsValid = false;

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
		const int32_t SHADOW_WIDTH = FDirectionalShadowDepthPass::kCascadeShadowResolution;
		const int32_t SHADOW_HEIGHT = FDirectionalShadowDepthPass::kCascadeShadowResolution * FDirectionalShadowFrustumFitter::kCascadeCount;
		d->DepthRenderBuffer = d->RHI->RHICreateRenderTarget(RenderCore::EPixelFormat::PF_R32_FLOAT, SHADOW_WIDTH, SHADOW_HEIGHT, 1, false, true);
		if (!d->PointShadowCube)
			d->PointShadowCube = d->RHI->RHICreateTextureCube(RenderCore::EPixelFormat::PF_R32_FLOAT, FPointShadowCubePass::kCubeFaceResolution,
															 FPointShadowCubePass::kCubeFaceResolution, 1, false);
		if (!d->SpotShadowBuffer)
			d->SpotShadowBuffer =
				d->RHI->RHICreateRenderTarget(RenderCore::EPixelFormat::PF_R32_FLOAT, FSpotShadowDepthPass::kSpotShadowTextureSize,
											  FSpotShadowDepthPass::kSpotShadowTextureSize, 1, false, true);
	}

	void ShadowRenderPass::InvalidateCachedMainLightForShading()
	{
		C_P(ShadowRenderPass);
		d->bCachedMainLightValid = false;
		d->CachedMainDirectionalShadowLightListIndex = -1;
		d->bCachedDirectionalCSMParamsValid = false;
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

	bool ShadowRenderPass::TryGetCachedDirectionalCSM(CBDirectionalShadowCSM& Out) const
	{
		C_P(const ShadowRenderPass);
		if (!d->bCachedDirectionalCSMParamsValid)
			return false;
		Out = d->CachedDirectionalCSM;
		return true;
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
		d->bCachedDirectionalCSMParamsValid = false;
		d->CachedDirectionalCSM = CBDirectionalShadowCSM{};
		d->bCachedPointShadowValid = false;
		d->bCachedSpotShadowValid = false;

		if (ShadowCasterMeshes.empty() && FrustumBoundsMeshes.empty() && !ShadowProjectorScene.bValid)
			return;

		const std::vector<GltfSceneMeshInfo>* subjectMeshList =
			FShadowSceneBounds::SelectShadowSubjectMeshListForFrustum(ShadowCasterMeshes, FrustumBoundsMeshes, ShadowProjectorScene);
		if (d->MeshDrawer)
			d->MeshDrawer->PruneStaleMeshShadowPasses(ShadowCasterMeshes, FrustumBoundsMeshes);

		const int mainDirIdx = FindFirstDirectionalLightIndex(Lights);
		const int pointShadowIdx = FindPointShadowCubeLightIndex(Lights);
		const int spotShadowIdx = FindSpotShadowLightIndex(Lights);

		math::AABB3 subjectWorldAabb;
		bool subjectValid = false;
		FShadowSceneBounds::BuildMergedShadowSubjectWorldAabb(subjectMeshList, ShadowProjectorScene, subjectWorldAabb, subjectValid);

		math::AABB3 receiverWorldAabb;
		bool receiverValid = false;
		FShadowSceneBounds::BuildMergedShadowReceiverWorldAabb(FrustumBoundsMeshes, receiverWorldAabb, receiverValid);

		if (mainDirIdx >= 0 && subjectValid && d->MeshDrawer)
		{
			FDirectionalShadowDepthPassOutputs dirOut{};
			FDirectionalShadowDepthPass::Render(RHIContext, ShadowCasterMeshes, Lights, mainDirIdx, subjectValid, subjectWorldAabb, receiverValid, receiverWorldAabb,
												ShadowProjectorScene, subjectMeshList, d->DepthRenderBuffer, *d->MeshDrawer, dirOut);
			d->CachedMainLightForShading = dirOut.CachedMainLightForShading;
			d->bCachedMainLightValid = dirOut.bCachedMainLightValid;
			d->CachedMainDirectionalShadowLightListIndex = dirOut.CachedMainDirectionalShadowLightListIndex;
			d->CachedDirectionalCSM = dirOut.CachedDirectionalCSM;
			d->bCachedDirectionalCSMParamsValid = dirOut.bCachedDirectionalCSMParamsValid;
		}

		if (pointShadowIdx >= 0 && d->PointShadowCube && !ShadowCasterMeshes.empty() && d->MeshDrawer)
		{
			FPointShadowCubePass::FOutputs ptOut{};
			FPointShadowCubePass::Render(RHIContext, ShadowCasterMeshes, Lights[static_cast<size_t>(pointShadowIdx)], pointShadowIdx, d->PointShadowCube, *d->MeshDrawer,
										 ptOut);
			for (int i = 0; i < 6; ++i)
				d->CachedPointFaceVP[i] = ptOut.CachedPointFaceVP[i];
			d->CachedPointLightPos = ptOut.CachedPointLightPos;
			d->CachedPointLightRange = ptOut.CachedPointLightRange;
			d->CachedPointShadowLightIndex = ptOut.CachedPointShadowLightIndex;
			d->bCachedPointShadowValid = ptOut.bCachedPointShadowValid;
		}

		if (spotShadowIdx >= 0 && d->SpotShadowBuffer && d->MeshDrawer)
		{
			const std::vector<GltfSceneMeshInfo>& spotMeshList = !ShadowCasterMeshes.empty() ? ShadowCasterMeshes : FrustumBoundsMeshes;
			if (!spotMeshList.empty())
			{
				Light& spotL = Lights[static_cast<size_t>(spotShadowIdx)];
				math::AABB3 spotZFarBounds{};
				bool spotZFarOk = false;
				if (subjectValid)
				{
					spotZFarBounds = subjectWorldAabb;
					spotZFarOk = true;
				}
				if (receiverValid)
				{
					spotZFarBounds = spotZFarOk ? spotZFarBounds.MergeAABB(receiverWorldAabb) : receiverWorldAabb;
					spotZFarOk = true;
				}
				FSpotShadowDepthPass::SetupSpotShadowViewProjection(spotL, spotZFarOk ? &spotZFarBounds : nullptr, spotZFarOk);
				FSpotShadowDepthPass::FOutputs spotOut{};
				FSpotShadowDepthPass::Render(RHIContext, spotMeshList, spotL, spotShadowIdx, d->SpotShadowBuffer, *d->MeshDrawer, spotOut);
				d->CachedSpotLightViewProj = spotOut.CachedSpotLightViewProj;
				d->CachedSpotLightView = spotOut.CachedSpotLightView;
				d->CachedSpotShadowLightIndex = spotOut.CachedSpotShadowLightIndex;
				d->bCachedSpotShadowValid = spotOut.bCachedSpotShadowValid;
			}
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
