#pragma once
#include "Render/MaterialPreFrame.h"
#include "math/matrix4x4.h"
#include "math/vector3.h"
#include <memory>
#include <vector>

namespace RenderCore
{
	class RHICommandContext;
	class RHITextureCube;
}

namespace Engine
{
	struct GltfSceneMeshInfo;
	class FShadowDepthMeshDrawer;

	/** UE-style: one point light cubemap shadow depth pass (six faces). */
	class FPointShadowCubePass
	{
	public:
		static constexpr int kCubeFaceResolution = 512;

		/** First point light that owns the single cubemap shadow slot (`kPointLightCubeShadowMapIndex`). */
		static int FindPointShadowCubeLightIndex(const std::vector<Light>& Lights);

		struct FOutputs
		{
			math::Matrix4x4 CachedPointFaceVP[6]{};
			math::Vector3 CachedPointLightPos{};
			float CachedPointLightRange = 0.f;
			int CachedPointShadowLightIndex = -1;
			bool bCachedPointShadowValid = false;
		};

		static void Render(RenderCore::RHICommandContext& RHIContext, const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const Light& PointLight, int PointLightIndex,
						  const std::shared_ptr<RenderCore::RHITextureCube>& PointShadowCube, FShadowDepthMeshDrawer& MeshDrawer, FOutputs& OutOutputs);
	};
}
