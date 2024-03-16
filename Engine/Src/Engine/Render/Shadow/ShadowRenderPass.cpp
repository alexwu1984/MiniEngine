#include "Render/Shadow/ShadowRenderPass.h"
#include "GltfModel/GltfMesh.h"
#include "Scene/GltfMeshComponent.h"
#include "Scene/CameraComponent.h"
#include "Engine/Render/SceneRender.h"
#include "Engine/Engine.h"
#include "Engine/Scene/SceneView.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICommandContext.h"
#include "Render/Shadow/ShadowPS.h"
#include "Render/Shadow/ShadowMapManager.h"
#include "Render/Shadow/ShadowMap.h"
#include "RHI/RHITexture2D.h"
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
		d->RHI->RHICreateRenderTarget(RenderCore::EPixelFormat::PF_ShadowDepth, SHADOW_WIDTH, SHADOW_HEIGHT, 1, false, true);
	}

	void ShadowRenderPass::Render(const std::vector<GltfSceneMeshInfo>& Meshes, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<SceneView> View)
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

		ENQUEUE_UNIQUE_RENDER_COMMAND([this,projActor, View, Meshes, &RHIContext](RenderCore::DynamicRHI* RHI) {
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
			math::Vector2 nearAndFar = { shadowmap->lsNear - 0.1f,shadowmap->lsFar + 0.1f };

			const auto& Lights = View->GetLights();
			Light mainLight = Lights[0];

			math::Vector3 lightLookAt; //此处没有考虑相机平移的情况
			math::Vector3 lightUp = math::Vector3::UnitY;
			math::Vector3 lightPos = lightLookAt + (mainLight.Direction * LIGHT_DISTANCE);

			math::Vector3 zAxis = (lightLookAt - lightPos).Normalize();
			if (math::Abs(math::Vector3::Dot(zAxis, lightUp)) > 0.999)
			{
				lightUp = { lightUp.z, lightUp.x, lightUp.y };
			}

			mainLight.LightView = math::Matrix4x4::MatrixLookAtLH(lightPos, lightLookAt, lightUp);
			mainLight.LightViewProj = mainLight.LightView * math::Matrix4x4::MatrixOrthographicOffCenterLH(l, r, b, t, nearAndFar.x, nearAndFar.y);

			RHIContext.SetRenderTarget(d->DepthRenderBuffer);
			for (auto itMeshInfo = Meshes.begin(); itMeshInfo != Meshes.end(); ++itMeshInfo)
			{
				for (auto itMesh = itMeshInfo->Meshes.begin(); itMesh != itMeshInfo->Meshes.end(); ++itMesh)
				{
					auto& shadowRender = d->ShadowRenders[*itMesh];
					if (!shadowRender)
					{
						shadowRender = std::make_shared<ShadowPS>(d->RHI, *itMesh);
						shadowRender->InitResource();
					}
					shadowRender->Draw(RHIContext, (*itMesh)->GetMeshMat() * itMeshInfo->WorldTransform, mainLight);
				}
			}
		});

	}

}