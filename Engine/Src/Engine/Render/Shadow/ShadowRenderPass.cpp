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

namespace Engine
{
	struct ShadowRenderPassPrivate
	{
		RenderCore::DynamicRHI* RHI;
		//std::shared_ptr<ShadowPS> ShadowRender;
		std::map <std::shared_ptr<GltfMesh>, std::shared_ptr< ShadowPS> > ShadowRenders;
		std::shared_ptr<ShadowMapManager> ShadowMgr;

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
	}

	void ShadowRenderPass::Render(const std::vector<GltfSceneMeshInfo>& Meshes, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<SceneView> View)
	{
		C_P(ShadowRenderPass);
		d->ShadowMgr->Update(View);
		for (auto itMeshs = Meshes.begin(); itMeshs != Meshes.end(); ++itMeshs)
		{
			for (auto itMesh = itMeshs->Meshes.begin(); itMesh != itMeshs->Meshes.end(); ++itMesh)
			{
				auto& shadowRender =  d->ShadowRenders[*itMesh];
				if (!shadowRender)
				{
					shadowRender = std::make_shared<ShadowPS>(d->RHI, *itMesh);
					shadowRender->InitResource();
				}
			}
		}
	}

}