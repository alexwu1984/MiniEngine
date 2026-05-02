#pragma once
#include "core/inc.h"
#include "Render/MaterialPreFrame.h"
#include "Render/Shadow/ShadowMap.h"
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
					RenderCore::RHICommandContext& RHIContext, std::vector<Light> Lights, const FShadowProjectorSceneData& ShadowProjectorScene);

		std::shared_ptr<RenderCore::RHIRenderTarget> GetShadowMap() const;

		/** Clears the last shadow pass light copy (e.g. when the shadow pass is skipped this frame). */
		void InvalidateCachedMainLightForShading();
		/** Last frame's main directional light after shadow pass (LightViewProj + ShadowMapIndex); for base pass CB. */
		bool TryGetCachedMainLightForShading(Light& OutLight);

		/** Drop per-mesh ShadowPS instances (VS/IL tied to vertex layout). Call after full scene swaps so layouts cannot alias recycled meshes. */
		void ClearCachedMeshShadowPasses();

	private:
		ShadowRenderPassPrivate* d_ptr = nullptr;
	};
}