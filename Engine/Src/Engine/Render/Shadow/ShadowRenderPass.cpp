#include "Render/Shadow/ShadowRenderPass.h"
#include "GltfModel/GltfMesh.h"
#include "Scene/GltfMeshComponent.h"
#include "Engine/Thread/RenderThread.h"
#include "Scene/Actor.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICommandContext.h"
#include "Render/Shadow/ShadowPS.h"
#include "Render/Shadow/ShadowMapManager.h"
#include "Render/Shadow/ShadowMap.h"
#include "RHI/RHIRenderTarget.h"
#include "math/aabb3.h"
#include "math/vector2.h"
#include <unordered_set>

namespace Engine
{
	static constexpr float LIGHT_DISTANCE = 4.0f;
	struct ShadowRenderPassPrivate
	{
		RenderCore::DynamicRHI* RHI;
		std::map<std::shared_ptr<MeshBase>, std::shared_ptr<ShadowPS>> ShadowRenders;
		std::shared_ptr<ShadowMapManager> ShadowMgr;
		std::shared_ptr<RenderCore::RHIRenderTarget> DepthRenderBuffer;

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

	void ShadowRenderPass::Render(const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const std::vector<GltfSceneMeshInfo>& FrustumBoundsMeshes,
								  [[maybe_unused]] RenderCore::RHICommandContext& RHIContext, std::vector<Light> Lights, std::shared_ptr<Actor> ShadowProjector,
								  const std::vector<std::shared_ptr<Actor>>& AllActorsForShadow)
	{
		C_P(ShadowRenderPass);
		d->ShadowMgr->Update(Lights, AllActorsForShadow);

		auto projActor = ShadowProjector;
		if (!projActor)
		{
			return;
		}

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

		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[this, projActor, Lights = std::move(Lights), ShadowCasterMeshes, boundsMeshes](RenderCore::DynamicRHI* RHI) mutable
			{
				C_P(ShadowRenderPass);

				auto shadowCtx = RHI->GetDefaultCommandContext();
				if (!shadowCtx)
					return;

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
						math::AABB3 wbox = Mesh->GetBoundingBox().Transform(Mesh->GetMeshMat() * MeshInfo.WorldTransform);
						mergedWorldAabb = mergedValid ? mergedWorldAabb.MergeAABB(wbox) : wbox;
						mergedValid = true;
					}
				}
				if (!mergedValid)
				{
					auto comp = projActor->GetComponent<GltfMeshComponent>();
					if (!comp)
						return;
					math::AABB3 modelBox = comp->GetModelBox();
					mergedWorldAabb = modelBox.Transform(projActor->GetWorldTransform());
				}

				math::Vector3 wsSceneCorners[8];
				mergedWorldAabb.GetPoint(wsSceneCorners);

				math::Vector3 lightLookAt = mergedWorldAabb.GetCenter();
				math::Vector3 lightUp = math::Vector3::UnitY;
				mainLight.Position = lightLookAt + (mainLight.Direction * LIGHT_DISTANCE);

				math::Vector3 zAxis = (lightLookAt - mainLight.Position).Normalize();
				if (math::Abs(math::Vector3::Dot(zAxis, lightUp)) > 0.999f)
				{
					lightUp = { lightUp.z, lightUp.x, lightUp.y };
				}

				mainLight.LightView = math::Matrix4x4::MatrixLookAtLH(mainLight.Position, lightLookAt, lightUp);

				math::Vector3 lsMin(FLT_MAX, FLT_MAX, FLT_MAX);
				math::Vector3 lsMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
				for (int i = 0; i < 8; ++i)
				{
					const math::Vector3& wsCorner = wsSceneCorners[i];
					math::Vector3 lsCorner = mainLight.LightView.TransformPosition(wsCorner);
					lsMin = math::Vector3((std::min)(lsMin.x, lsCorner.x), (std::min)(lsMin.y, lsCorner.y), (std::min)(lsMin.z, lsCorner.z));
					lsMax = math::Vector3((std::max)(lsMax.x, lsCorner.x), (std::max)(lsMax.y, lsCorner.y), (std::max)(lsMax.z, lsCorner.z));
				}

				const float zMargin = 4.0f;
				float zLo = lsMin.z;
				float zHi = lsMax.z;
				if (zLo > zHi)
				{
					const float t = zLo;
					zLo = zHi;
					zHi = t;
				}
				float nearValue = zLo - zMargin;
				float farValue = zHi + zMargin;
				if (nearValue >= farValue)
					farValue = nearValue + 1.0f;

				const float xyMargin = 0.22f;
				float sizeX = (lsMax.x - lsMin.x) * 0.5f * (1.0f + xyMargin);
				float sizeY = (lsMax.y - lsMin.y) * 0.5f * (1.0f + xyMargin);
				float centerX = (lsMin.x + lsMax.x) * 0.5f;
				float centerY = (lsMin.y + lsMax.y) * 0.5f;

				mainLight.LightViewProj = mainLight.LightView * math::Matrix4x4::MatrixOrthographicOffCenterLH(centerX - sizeX, centerX + sizeX, centerY - sizeY, centerY + sizeY, nearValue, farValue);

				shadowCtx->Clear(d->DepthRenderBuffer, core::FLinearColor::White, 1.f, 0);
				auto TargetSize = d->DepthRenderBuffer->GetSize();
				shadowCtx->SetViewPort(0, 0, TargetSize.x, TargetSize.y);

				for (const auto& MeshInfo : ShadowCasterMeshes)
				{
					size_t MeshSize = MeshInfo.Meshes.size();
					for (int32_t MeshIndex = 0; MeshIndex < MeshSize; ++MeshIndex)
					{
						std::shared_ptr<MeshBase> Mesh = MeshInfo.Meshes[MeshIndex];
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

						shadowRender->Draw(*shadowCtx, Mesh->GetMeshMat() * MeshInfo.WorldTransform, mainLight, d->DepthRenderBuffer);
					}
				}
			},
			false);
	}

	std::shared_ptr<RenderCore::RHIRenderTarget> ShadowRenderPass::GetShadowMap() const
	{
		C_P(ShadowRenderPass);
		return d->DepthRenderBuffer;
	}

} // namespace Engine
