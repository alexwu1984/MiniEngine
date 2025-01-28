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
		//std::shared_ptr<ShadowPS> ShadowRender;
		std::map <std::shared_ptr<GltfMesh>, std::shared_ptr< ShadowPS> > ShadowRenders;
		std::shared_ptr<ShadowMapManager> ShadowMgr;
		std::shared_ptr<RenderCore::RHIRenderTarget> DepthRenderBuffer;

		ShadowRenderPassPrivate(RenderCore::DynamicRHI* _RHI)
			:RHI(_RHI)
		{
			//ShadowRender = std::make_shared<ShadowPS>(RHI);
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
		const int32_t SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
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

			math::Vector3 lightLookAt; //此处没有考虑相机平移的情况
			math::Vector3 lightUp = math::Vector3::UnitY;
			mainLight.Position = lightLookAt + (mainLight.Direction * LIGHT_DISTANCE);

			math::Vector3 zAxis = (lightLookAt - mainLight.Position).Normalize();
			if (math::Abs(math::Vector3::Dot(zAxis, lightUp)) > 0.999)
			{
				lightUp = { lightUp.z, lightUp.x, lightUp.y };
			}

			mainLight.LightView = math::Matrix4x4::MatrixLookAtLH(mainLight.Position, lightLookAt, lightUp);
			// glm::ortho(-m_CameraSizeExtent, m_CameraSizeExtent, -m_CameraSizeExtent, m_CameraSizeExtent, 0.1f, 7.5f);
			//mainLight.LightViewProj = mainLight.LightView * math::Matrix4x4::MatrixOrthographicOffCenterLH(l, r, b, t, nearValue, farValue);
			float CameraSizeExtent = 1.0f;
			mainLight.LightViewProj = mainLight.LightView * math::Matrix4x4::MatrixOrthographicOffCenterLH(-CameraSizeExtent, CameraSizeExtent, -CameraSizeExtent, CameraSizeExtent, nearValue, farValue);

			RHIContext.SetRenderTarget(d->DepthRenderBuffer);
			RHIContext.Clear(d->DepthRenderBuffer, core::FLinearColor::White, 1.f, 0);
			auto TargetSize = d->DepthRenderBuffer->GetSize();
			RHIContext.SetViewPort(0, 0, TargetSize.x, TargetSize.y);

			for (const auto& MeshInfo : MeshInfos)
			{
				size_t MeshSize = MeshInfo.Meshes.size();
				for (int32_t MeshIndex = 0; MeshIndex < MeshSize; ++MeshIndex)
				{
					std::shared_ptr<GltfMesh> Mesh = MeshInfo.Meshes[MeshIndex];
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

					shadowRender->Draw(RHIContext, Mesh->GetMeshMat() * MeshInfo.WorldTransform, mainLight);
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