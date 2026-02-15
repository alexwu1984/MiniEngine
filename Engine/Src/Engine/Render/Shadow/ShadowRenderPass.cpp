#include "Render/Shadow/ShadowRenderPass.h"
#include "GltfModel/GltfMesh.h"
#include "Scene/GltfMeshComponent.h"
#include "Scene/CameraComponent.h"
#include "Engine/Render/SceneRender.h"
#include "Engine/Thread/RenderThread.h"
#include "Engine/Engine.h"
#include "Engine/Scene/SceneView.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICommandContext.h"
#include "Render/Shadow/ShadowPS.h"
#include "Render/Shadow/ShadowMapManager.h"
#include "Render/Shadow/ShadowMap.h"
#include "RHI/RHIRenderTarget.h"
#include "Engine/Scene/Actor.h"
#include "math/aabb3.h"
#include "math/vector2.h"

namespace Engine
{
	static constexpr float LIGHT_DISTANCE = 4.0f;
	struct ShadowRenderPassPrivate
	{
		RenderCore::DynamicRHI* RHI;
		std::map <std::shared_ptr<MeshBase>, std::shared_ptr< ShadowPS> > ShadowRenders;
		std::shared_ptr<ShadowMapManager> ShadowMgr;
		std::shared_ptr<RenderCore::RHIRenderTarget> DepthRenderBuffer;

		ShadowRenderPassPrivate(RenderCore::DynamicRHI* _RHI)
			:RHI(_RHI)
		{
			ShadowMgr = std::make_shared<ShadowMapManager>();
			ShadowMgr->SetShadowCascades(1);
		}
	};

	ShadowRenderPass::ShadowRenderPass(RenderCore::DynamicRHI* RHI)
		:d_ptr(new ShadowRenderPassPrivate(RHI))
	{

	}

	ShadowRenderPass::~ShadowRenderPass()
	{
		delete d_ptr;
	}

	void ShadowRenderPass::InitResource()
	{
		C_P(ShadowRenderPass);
		// 提高分辨率到 2048×2048 以获得更好的阴影质量
		const int32_t SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
		d->DepthRenderBuffer = d->RHI->RHICreateRenderTarget(RenderCore::EPixelFormat::PF_FloatRGBA, SHADOW_WIDTH, SHADOW_HEIGHT, 1, false, true);
	}

	void ShadowRenderPass::Render(const std::vector<GltfSceneMeshInfo>& MeshInfos, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<SceneView> View)
	{
		C_P(ShadowRenderPass);
		d->ShadowMgr->Update(View);

		std::shared_ptr<Actor> projActor;
		auto actors = View->GetAllActors();
		for (auto actor : actors)
		{
			if (actor->IsProjectShadow())
			{
				projActor = actor;
				break;
			}
		}
		if (!projActor)
		{
			return;
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND([this,projActor, View, MeshInfos, &RHIContext](RenderCore::DynamicRHI* RHI) {
			C_P(ShadowRenderPass);

			auto modelBox = projActor->GetComponent<GltfMeshComponent>()->GetModelBox();
			math::Vector3 minPoint = modelBox.GetMinPoint();
			math::Vector3 maxPoint = modelBox.GetMaxPoint();
			float l = minPoint.x;
			float b = minPoint.y;
			float n = minPoint.z;
			float r = maxPoint.x;
			float t = maxPoint.y;
			float f = maxPoint.z;

			auto shadowmap = d->ShadowMgr->GetShadowMap(0);
			float nearValue = shadowmap->lsNear - 0.1f;
			float farValue = shadowmap->lsFar + 0.1f;

			auto& Lights = View->GetLights();
			Light& mainLight = Lights[0];
			mainLight.ShadowMapIndex = 0;

			// 计算场景包围盒在世界空间中的8个角点（为动态计算Light Frustum做准备）
			math::Matrix4x4 worldTransform = projActor->GetWorldTransform();
			math::Vector3 wsSceneCorners[8];
			modelBox.GetPoint(wsSceneCorners);
			
			// 将包围盒角点转换到世界空间
			math::Vector3 wsMin(FLT_MAX, FLT_MAX, FLT_MAX);
			math::Vector3 wsMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			for (int i = 0; i < 8; ++i)
			{
				math::Vector3 wsCorner = worldTransform.TransformPosition(wsSceneCorners[i]);
				wsMin = math::Vector3((std::min)(wsMin.x, wsCorner.x), (std::min)(wsMin.y, wsCorner.y), (std::min)(wsMin.z, wsCorner.z));
				wsMax = math::Vector3((std::max)(wsMax.x, wsCorner.x), (std::max)(wsMax.y, wsCorner.y), (std::max)(wsMax.z, wsCorner.z));
			}
			
			// 计算光照中心点（包围盒中心）
			math::Vector3 lightLookAt = (wsMin + wsMax) * 0.5f; 
			math::Vector3 lightUp = math::Vector3::UnitY;
			mainLight.Position = lightLookAt + (mainLight.Direction * LIGHT_DISTANCE);

			math::Vector3 zAxis = (lightLookAt - mainLight.Position).Normalize();
			if (math::Abs(math::Vector3::Dot(zAxis, lightUp)) > 0.999)
			{
				lightUp = { lightUp.z, lightUp.x, lightUp.y };
			}

			mainLight.LightView = math::Matrix4x4::MatrixLookAtLH(mainLight.Position, lightLookAt, lightUp);
			
			// 动态计算 Light Frustum：将场景包围盒转换到光照空间，然后计算正交投影范围
			math::Vector3 lsMin(FLT_MAX, FLT_MAX, FLT_MAX);
			math::Vector3 lsMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			
			for (int i = 0; i < 8; ++i)
			{
				math::Vector3 wsCorner = worldTransform.TransformPosition(wsSceneCorners[i]);
				// 转换到光照空间（view space）
				math::Vector3 lsCorner = mainLight.LightView.TransformPosition(wsCorner);
				lsMin = math::Vector3((std::min)(lsMin.x, lsCorner.x), (std::min)(lsMin.y, lsCorner.y), (std::min)(lsMin.z, lsCorner.z));
				lsMax = math::Vector3((std::max)(lsMax.x, lsCorner.x), (std::max)(lsMax.y, lsCorner.y), (std::max)(lsMax.z, lsCorner.z));
			}
			
			// 使用光照空间的包围盒来设置正交投影，并添加一些边距以防止阴影裁切
			float margin = 0.1f;
			float sizeX = (lsMax.x - lsMin.x) * 0.5f * (1.0f + margin);
			float sizeY = (lsMax.y - lsMin.y) * 0.5f * (1.0f + margin);
			float centerX = (lsMin.x + lsMax.x) * 0.5f;
			float centerY = (lsMin.y + lsMax.y) * 0.5f;
			
			mainLight.LightViewProj = mainLight.LightView * math::Matrix4x4::MatrixOrthographicOffCenterLH(
				centerX - sizeX, centerX + sizeX,
				centerY - sizeY, centerY + sizeY,
				nearValue, farValue);

			RHIContext.Clear(d->DepthRenderBuffer, core::FLinearColor::White, 1.f, 0);
			auto TargetSize = d->DepthRenderBuffer->GetSize();
			RHIContext.SetViewPort(0, 0, TargetSize.x, TargetSize.y);

			for (const auto& MeshInfo : MeshInfos)
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

					shadowRender->Draw(RHIContext, Mesh->GetMeshMat() * MeshInfo.WorldTransform, mainLight, d->DepthRenderBuffer);
				}
			}
		});

	}

	std::shared_ptr<RenderCore::RHIRenderTarget> ShadowRenderPass::GetShadowMap() const
	{
		C_P(ShadowRenderPass);
		return d->DepthRenderBuffer;
	}

}