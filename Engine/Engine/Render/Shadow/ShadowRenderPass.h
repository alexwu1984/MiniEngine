#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
}

namespace Engine
{
	class  SceneView;
	struct GltfSceneMeshInfo;
	struct ShadowRenderPassPrivate;

	class ShadowRenderPass
	{
	public:
		ShadowRenderPass(RenderCore::DynamicRHI* RHI);
		~ShadowRenderPass();

		void InitResource();
		void Render(const std::vector<GltfSceneMeshInfo>& MeshesPair,
			RenderCore::RHICommandContext& RHIContext, std::shared_ptr<SceneView> View);

	private:
		ShadowRenderPassPrivate* d_ptr = nullptr;
	};
}