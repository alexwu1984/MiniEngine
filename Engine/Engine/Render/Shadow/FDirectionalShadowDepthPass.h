#pragma once
#include "Render/MaterialPreFrame.h"
#include "Render/Shadow/ShadowProjectorTypes.h"
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

	/** Outputs written by the directional + CSM depth pass for deferred / base pass consumers. */
	struct FDirectionalShadowDepthPassOutputs
	{
		Light CachedMainLightForShading{};
		bool bCachedMainLightValid = false;
		int CachedMainDirectionalShadowLightListIndex = -1;
		CBDirectionalShadowCSM CachedDirectionalCSM{};
		bool bCachedDirectionalCSMParamsValid = false;
	};

	/** UE-style: directional shadow map (single atlas or CSM tiles) + CSM uniform fill. */
	class FDirectionalShadowDepthPass
	{
	public:
		static constexpr int kCascadeShadowResolution = 2048;

		/** First directional in the view light list (ortho / CSM shadow applies to this slot only). */
		static int FindFirstDirectionalLightIndex(const std::vector<Light>& Lights);

		static void Render(RenderCore::RHICommandContext& RHIContext, const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, std::vector<Light>& Lights,
							int MainDirLightIndex, bool bSubjectValid, const math::AABB3& SubjectWorldAabb, bool bReceiverValid, const math::AABB3& ReceiverWorldAabb,
							const FShadowProjectorSceneData& ShadowProjectorScene, const std::vector<GltfSceneMeshInfo>* SubjectMeshListForFrustumDriver,
							const std::shared_ptr<RenderCore::RHIRenderTarget>& DepthRenderBuffer, FShadowDepthMeshDrawer& MeshDrawer,
							FDirectionalShadowDepthPassOutputs& OutOutputs);
	};
}
