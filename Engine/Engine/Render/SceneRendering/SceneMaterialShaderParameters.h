#pragma once
#include "Render/MaterialRender.h"

namespace Engine
{
	class FWorldSceneRender;
	struct FSceneViewData;
	class GBuffer;
	class MeshBase;

	/** Builds per-draw shader parameter payloads for deferred base pass materials. */
	class FSceneMaterialShaderParameters
	{
	public:
		static MaterialRenderParam BuildForDeferredBasePass(const FWorldSceneRender* WorldSceneRender, const FSceneViewData* ViewData, const MeshBase* Mesh,
															const math::Matrix4x4& WorldTransform, const math::Matrix4x4& PrevWorldTransform,
															const std::shared_ptr<GBuffer>& TargetBuffer);
	};
}
