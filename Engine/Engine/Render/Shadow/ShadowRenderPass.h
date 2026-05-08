#pragma once
#include "core/inc.h"
#include "math/vector4.h"
#include "Render/MaterialPreFrame.h"
#include "Render/Shadow/ShadowMap.h"
#include <memory>
#include <vector>

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHIRenderTarget;
	class RHITextureCube;
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
		std::shared_ptr<RenderCore::RHITextureCube> GetPointShadowCube() const;
		std::shared_ptr<RenderCore::RHIRenderTarget> GetSpotShadowMap() const;

		/** Clears cached directional + point cubemap shadow data when the shadow pass is skipped or invalidated. */
		void InvalidateCachedMainLightForShading();
		/** Last frame's main directional light after shadow pass (LightViewProj + ShadowMapIndex); for base pass CB. */
		bool TryGetCachedMainLightForShading(Light& OutLight);

		/** Fills FaceVP / Light index / xyz+range.w after point cubemap shadow render; false if no cached cube pass. */
		bool TryGetCachedPointShadowForDeferred(int& OutLightIndex, math::Matrix4x4 OutFaceVp[6], math::Vector4& OutPosRange) const;

		/** After spotlight depth pass: light list index + view-projection used for deferred PCF (row-vector world * VP). */
		bool TryGetCachedSpotShadowForDeferred(int& OutLightIndex, math::Matrix4x4& OutSpotLightViewProj) const;

		/** Drop per-mesh ShadowPS instances (VS/IL tied to vertex layout). Call after full scene swaps so layouts cannot alias recycled meshes. */
		void ClearCachedMeshShadowPasses();

	private:
		ShadowRenderPassPrivate* d_ptr = nullptr;
	};
}