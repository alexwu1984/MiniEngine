#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHIRenderTarget;
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
		// ShadowCasterMeshes: drawn into the shadow map (ProjShadow actors only).
		// FrustumBoundsMeshes: union AABB for light frustum; use all visible receivers (e.g. floor) or casters-only breaks shadows on large surfaces.
		void Render(const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes,
			const std::vector<GltfSceneMeshInfo>& FrustumBoundsMeshes,
			RenderCore::RHICommandContext& RHIContext, std::shared_ptr<SceneView> View);

		std::shared_ptr<RenderCore::RHIRenderTarget> GetShadowMap() const;

	private:
		ShadowRenderPassPrivate* d_ptr = nullptr;
	};
}