#pragma once
#include "Render/MaterialPreFrame.h"
#include "math/aabb3.h"

namespace RenderCore
{
	class RHICommandContext;
	class RHIRenderTarget;
}

namespace Engine
{
	struct GltfSceneMeshInfo;
	class FShadowDepthMeshDrawer;

	/** UE-style: one spotlight depth map pass. */
	class FSpotShadowDepthPass
	{
	public:
		static constexpr int kSpotShadowTextureSize = 2048;

		/** First spotlight that owns the single spot shadow depth slot (`kSpotLightShadowMapIndex`). */
		static int FindSpotShadowLightIndex(const std::vector<Light>& Lights);

		struct FOutputs
		{
			math::Matrix4x4 CachedSpotLightViewProj{};
			math::Matrix4x4 CachedSpotLightView{};
			int CachedSpotShadowLightIndex = -1;
			bool bCachedSpotShadowValid = false;
		};

		static void SetupSpotShadowViewProjection(Light& spotLight, const math::AABB3* pSceneBoundsWorld, bool bSceneBoundsValid);

		/** UE-style parameters: setup (view-proj from bounds) + rasterize depth in one call (B initializer-style batching). */
		struct FSpotShadowDepthPassParameters
		{
			RenderCore::RHICommandContext* RHICmdList = nullptr;
			const std::vector<GltfSceneMeshInfo>* ShadowCasterMeshes = nullptr;
			const std::vector<GltfSceneMeshInfo>* FrustumBoundsMeshes = nullptr;
			std::vector<Light>* FrameLights = nullptr;
			int SpotLightListIndex = -1;
			bool bSubjectValid = false;
			math::AABB3 SubjectWorldAabb{};
			bool bReceiverValid = false;
			math::AABB3 ReceiverWorldAabb{};
			std::shared_ptr<RenderCore::RHIRenderTarget> SpotShadowBuffer{};
			FShadowDepthMeshDrawer* MeshDrawer = nullptr;
			FOutputs* OutOutputs = nullptr;
		};

		static void Render(const FSpotShadowDepthPassParameters& Params);
	};
}
