#include "Render/Shadow/ShadowRenderPass.h"
#include "GltfModel/GltfMesh.h"
#include "Scene/GltfMeshComponent.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICommandContext.h"
#include "Render/Shadow/ShadowPS.h"
#include "Render/Shadow/ShadowMapManager.h"
#include "Render/Shadow/ShadowMap.h"
#include "RHI/RHIRenderTarget.h"
#include "math/aabb3.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include <cmath>
#include <unordered_set>

namespace
{
	static constexpr float kZMargin = 4.0f;
	static constexpr float kXYMargin = 0.22f;
	// Fit / snap iterations: snap changes LightView, which changes light-space bounds; need another fit+snap with updated half-extents.
	static constexpr int kFitSnapIterations = 2;

	static void FitOrthoFromWorldCorners(
		const math::Vector3 wsSceneCorners[8],
		const math::Matrix4x4& lightView,
		float& nearValue,
		float& farValue,
		float& centerX,
		float& centerY,
		float& sizeX,
		float& sizeY)
	{
		math::Vector3 lsMin(FLT_MAX, FLT_MAX, FLT_MAX);
		math::Vector3 lsMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (int i = 0; i < 8; ++i)
		{
			math::Vector3 lsCorner = lightView.TransformPosition(wsSceneCorners[i]);
			lsMin = math::Vector3((std::min)(lsMin.x, lsCorner.x), (std::min)(lsMin.y, lsCorner.y), (std::min)(lsMin.z, lsCorner.z));
			lsMax = math::Vector3((std::max)(lsMax.x, lsCorner.x), (std::max)(lsMax.y, lsCorner.y), (std::max)(lsMax.z, lsCorner.z));
		}
		float zLo = lsMin.z;
		float zHi = lsMax.z;
		if (zLo > zHi)
		{
			const float t = zLo;
			zLo = zHi;
			zHi = t;
		}
		nearValue = zLo - kZMargin;
		farValue = zHi + kZMargin;
		if (nearValue >= farValue)
			farValue = nearValue + 1.0f;
		sizeX = (lsMax.x - lsMin.x) * 0.5f * (1.0f + kXYMargin);
		sizeY = (lsMax.y - lsMin.y) * 0.5f * (1.0f + kXYMargin);
		centerX = (lsMin.x + lsMax.x) * 0.5f;
		centerY = (lsMin.y + lsMax.y) * 0.5f;
	}

	static void SnapLightViewTranslationToShadowTexels(
		math::Matrix4x4& lightView,
		const math::Vector3& refWorld,
		float sizeX,
		float sizeY,
		int32_t shadowW,
		int32_t shadowH)
	{
		const float shW = static_cast<float>((std::max)(shadowW, 1));
		const float shH = static_cast<float>((std::max)(shadowH, 1));
		const float texelLvX = (2.0f * sizeX) / shW;
		const float texelLvY = (2.0f * sizeY) / shH;
		if (texelLvX <= 1e-8f || texelLvY <= 1e-8f)
			return;
		const math::Vector3 lvRef = lightView.TransformPosition(refWorld);
		const float targetX = std::floor(lvRef.x / texelLvX + 0.5f) * texelLvX;
		const float targetY = std::floor(lvRef.y / texelLvY + 0.5f) * texelLvY;
		lightView._30 += targetX - lvRef.x;
		lightView._31 += targetY - lvRef.y;
	}

	// After View*Proj, nudge LightView so the reference point projects to integer shadow-map pixels (same UV remap as PBRMaterial.hlsl ComputeShadow).
	static void RefineLightViewFromClipTexelSnap(
		math::Matrix4x4& lightView,
		const math::Matrix4x4& proj,
		math::Matrix4x4& outViewProj,
		const math::Vector3& refWorld,
		float sizeX,
		float sizeY,
		int32_t shadowW,
		int32_t shadowH)
	{
		outViewProj = lightView * proj;
		const math::Vector4 rw(refWorld.x, refWorld.y, refWorld.z, 1.f);
		math::Vector4 clip = rw * outViewProj;
		const float iw = (std::fabs(clip.w) > 1e-8f) ? (1.f / clip.w) : 1.f;
		const float ndcX = clip.x * iw;
		const float ndcY = clip.y * iw;
		const float u = ndcX * 0.5f + 0.5f;
		const float v = ndcY * -0.5f + 0.5f;
		const float shW = static_cast<float>((std::max)(shadowW, 1));
		const float shH = static_cast<float>((std::max)(shadowH, 1));
		const float px = u * shW;
		const float py = v * shH;
		const float sx = std::floor(px + 0.5f);
		const float sy = std::floor(py + 0.5f);
		const float dPx = sx - px;
		const float dPy = sy - py;
		const float dNdcX = dPx / shW * 2.f;
		const float dNdcY = -dPy / shH * 2.f;
		lightView._30 += dNdcX * sizeX;
		lightView._31 += dNdcY * sizeY;
		outViewProj = lightView * proj;
	}
}

namespace Engine
{
	static constexpr float LIGHT_DISTANCE = 4.0f;
	struct ShadowRenderPassPrivate
	{
		RenderCore::DynamicRHI* RHI;
		std::map<std::shared_ptr<MeshBase>, std::shared_ptr<ShadowPS>> ShadowRenders;
		std::shared_ptr<ShadowMapManager> ShadowMgr;
		std::shared_ptr<RenderCore::RHIRenderTarget> DepthRenderBuffer;
		Light CachedMainLightForShading{};
		bool bCachedMainLightValid = false;

		ShadowRenderPassPrivate(RenderCore::DynamicRHI* _RHI)
			: RHI(_RHI)
		{
			ShadowMgr = std::make_shared<ShadowMapManager>();
			ShadowMgr->SetShadowCascades(1);
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
		const int32_t SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
		d->DepthRenderBuffer = d->RHI->RHICreateRenderTarget(RenderCore::EPixelFormat::PF_FloatRGBA, SHADOW_WIDTH, SHADOW_HEIGHT, 1, false, true);
	}

	void ShadowRenderPass::InvalidateCachedMainLightForShading()
	{
		C_P(ShadowRenderPass);
		d->bCachedMainLightValid = false;
	}

	bool ShadowRenderPass::TryGetCachedMainLightForShading(Light& OutLight)
	{
		C_P(ShadowRenderPass);
		if (!d->bCachedMainLightValid)
			return false;
		OutLight = d->CachedMainLightForShading;
		return true;
	}

	void ShadowRenderPass::Render(const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const std::vector<GltfSceneMeshInfo>& FrustumBoundsMeshes,
								  RenderCore::RHICommandContext& RHIContext, std::vector<Light> Lights, const FShadowProjectorSceneData& ShadowProjectorScene)
	{
		C_P(ShadowRenderPass);
		d->bCachedMainLightValid = false;
		d->ShadowMgr->Update(Lights, ShadowProjectorScene);

		if (ShadowCasterMeshes.empty() && !ShadowProjectorScene.bValid)
			return;

		const std::vector<GltfSceneMeshInfo>& boundsMeshes = FrustumBoundsMeshes.empty() ? ShadowCasterMeshes : FrustumBoundsMeshes;

		std::unordered_set<const MeshBase*> casterMeshPtrs;
		for (const auto& MeshInfo : ShadowCasterMeshes)
		{
			for (const auto& Mesh : MeshInfo.Meshes)
			{
				if (Mesh)
					casterMeshPtrs.insert(Mesh.get());
			}
		}
		for (auto it = d->ShadowRenders.begin(); it != d->ShadowRenders.end();)
		{
			if (!it->first || casterMeshPtrs.find(it->first.get()) == casterMeshPtrs.end())
				it = d->ShadowRenders.erase(it);
			else
				++it;
		}

		// Record on the same RHI command context as the frame graph pass (no nested render-thread enqueue).
		if (Lights.empty())
			return;

		Light& mainLight = Lights[0];
		mainLight.ShadowMapIndex = 0;

		math::AABB3 mergedWorldAabb;
		bool mergedValid = false;
		for (const auto& MeshInfo : boundsMeshes)
		{
			for (const auto& Mesh : MeshInfo.Meshes)
			{
				if (!Mesh)
					continue;
				// Frustum fit: actor world only. MeshMat includes per-frame node/skin pose; merging it into
				// the light AABB shifts LightViewProj every frame and drags unrelated receivers' shadows.
				// Shadow draws still use MeshMat * WorldTransform below.
				math::AABB3 wbox = Mesh->GetBoundingBox().Transform(MeshInfo.WorldTransform);
				mergedWorldAabb = mergedValid ? mergedWorldAabb.MergeAABB(wbox) : wbox;
				mergedValid = true;
			}
		}
		if (!mergedValid)
		{
			mergedWorldAabb = ShadowProjectorScene.ModelLocalAABB.Transform(ShadowProjectorScene.WorldTransform);
		}

		math::Vector3 wsSceneCorners[8];
		mergedWorldAabb.GetPoint(wsSceneCorners);

		const math::Vector3 lightLookAt = mergedWorldAabb.GetCenter();
		math::Vector3 lightUp = math::Vector3::UnitY;
		mainLight.Position = lightLookAt + (mainLight.Direction * LIGHT_DISTANCE);

		math::Vector3 zAxis = (lightLookAt - mainLight.Position).Normalize();
		if (math::Abs(math::Vector3::Dot(zAxis, lightUp)) > 0.999f)
		{
			lightUp = { lightUp.z, lightUp.x, lightUp.y };
		}

		mainLight.LightView = math::Matrix4x4::MatrixLookAtLH(mainLight.Position, lightLookAt, lightUp);

		float nearValue = 0.f;
		float farValue = 1.f;
		float centerX = 0.f;
		float centerY = 0.f;
		float sizeX = 1.f;
		float sizeY = 1.f;

		const core::vec2i smSize = d->DepthRenderBuffer ? d->DepthRenderBuffer->GetSize() : core::vec2i{ 2048, 2048 };

		for (int iter = 0; iter < kFitSnapIterations; ++iter)
		{
			FitOrthoFromWorldCorners(wsSceneCorners, mainLight.LightView, nearValue, farValue, centerX, centerY, sizeX, sizeY);
			SnapLightViewTranslationToShadowTexels(mainLight.LightView, lightLookAt, sizeX, sizeY, smSize.x, smSize.y);
		}
		FitOrthoFromWorldCorners(wsSceneCorners, mainLight.LightView, nearValue, farValue, centerX, centerY, sizeX, sizeY);

		const math::Matrix4x4 proj = math::Matrix4x4::MatrixOrthographicOffCenterLH(centerX - sizeX, centerX + sizeX, centerY - sizeY, centerY + sizeY, nearValue, farValue);
		RefineLightViewFromClipTexelSnap(mainLight.LightView, proj, mainLight.LightViewProj, lightLookAt, sizeX, sizeY, smSize.x, smSize.y);

		d->CachedMainLightForShading = mainLight;
		d->bCachedMainLightValid = true;

		RHIContext.Clear(d->DepthRenderBuffer, core::FLinearColor::White, 1.f, 0);
		auto TargetSize = d->DepthRenderBuffer->GetSize();
		RHIContext.SetViewPort(0, 0, TargetSize.x, TargetSize.y);

		for (const auto& MeshInfo : ShadowCasterMeshes)
		{
			size_t MeshSize = MeshInfo.Meshes.size();
			for (int32_t MeshIndex = 0; MeshIndex < MeshSize; ++MeshIndex)
			{
				std::shared_ptr<MeshBase> Mesh = MeshInfo.Meshes[MeshIndex];
				if (!Mesh)
					continue;
				auto& shadowRender = d->ShadowRenders[Mesh];
				if (!shadowRender)
				{
					shadowRender = std::make_shared<ShadowPS>(d->RHI, Mesh);
					shadowRender->InitResource();
				}
				if (Mesh->GetSkinId() > -1 && Mesh->GetBoneNodeArray().size() > 0)
				{
					auto& Bone = Mesh->GetBoneNodeArray()[Mesh->GetSkinId()];
					for (uint32_t BoneIndex = 0; BoneIndex < Bone.size(); BoneIndex++)
					{
						shadowRender->SetBoneMatrix(Bone[BoneIndex].FinalMat, BoneIndex);
					}
				}

				shadowRender->Draw(RHIContext, Mesh->GetMeshMat() * MeshInfo.WorldTransform, mainLight, d->DepthRenderBuffer);
			}
		}
	}

	std::shared_ptr<RenderCore::RHIRenderTarget> ShadowRenderPass::GetShadowMap() const
	{
		C_P(ShadowRenderPass);
		return d->DepthRenderBuffer;
	}

} // namespace Engine
