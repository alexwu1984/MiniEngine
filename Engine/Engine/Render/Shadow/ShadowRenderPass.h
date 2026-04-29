#pragma once
#include "core/inc.h"
#include "Render/MaterialPreFrame.h"
#include <memory>
#include <vector>

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHIRenderTarget;
}

namespace Engine
{
	class Actor;
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
		void Render(const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const std::vector<GltfSceneMeshInfo>& FrustumBoundsMeshes,
					[[maybe_unused]] RenderCore::RHICommandContext& RHIContext, std::vector<Light> Lights, std::shared_ptr<Actor> ShadowProjector,
					const std::vector<std::shared_ptr<Actor>>& AllActorsForShadow);

		std::shared_ptr<RenderCore::RHIRenderTarget> GetShadowMap() const;

	private:
		ShadowRenderPassPrivate* d_ptr = nullptr;
	};
}