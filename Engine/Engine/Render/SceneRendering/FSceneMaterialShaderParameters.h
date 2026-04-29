#pragma once
#include "Render/MaterialRender.h"

namespace Engine
{
	class SceneRender;
	class SceneView;
	class CameraComponent;
	class GBuffer;
	class MeshBase;

	/** Builds per-draw shader parameter payloads for deferred base pass materials. */
	class FSceneMaterialShaderParameters
	{
	public:
		static MaterialRenderParam BuildForDeferredBasePass(const SceneRender* SceneRender, const SceneView* View, CameraComponent* Camera,
															const MeshBase* Mesh, const math::Matrix4x4& WorldTransform, const math::Matrix4x4& PrevWorldTransform,
															const std::shared_ptr<GBuffer>& TargetBuffer, float EnvironmentRotatePitchDegrees,
															float EnvironmentRotateYawDegrees);
	};
}
