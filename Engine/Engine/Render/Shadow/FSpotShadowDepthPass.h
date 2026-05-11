#pragma once
#include "Render/MaterialPreFrame.h"
#include "math/aabb3.h"
#include <memory>
#include <vector>

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

		struct FOutputs
		{
			math::Matrix4x4 CachedSpotLightViewProj{};
			math::Matrix4x4 CachedSpotLightView{};
			int CachedSpotShadowLightIndex = -1;
			bool bCachedSpotShadowValid = false;
		};

		static void SetupSpotShadowViewProjection(Light& spotLight, const math::AABB3* pSceneBoundsWorld, bool bSceneBoundsValid);

		static void Render(RenderCore::RHICommandContext& RHIContext, const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, Light& SpotLight, int SpotLightIndex,
						   const std::shared_ptr<RenderCore::RHIRenderTarget>& SpotShadowBuffer, FShadowDepthMeshDrawer& MeshDrawer, FOutputs& OutOutputs);
	};
}
