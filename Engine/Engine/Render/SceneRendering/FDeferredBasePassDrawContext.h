#pragma once
#include <memory>

namespace Engine
{
	class SceneView;
	class GBuffer;
	class SceneRender;

	/** View and scene bindings consumed while recording deferred base pass draws. */
	struct FDeferredBasePassDrawContext
	{
		std::shared_ptr<SceneView> View;
		std::shared_ptr<GBuffer> TargetBuffer;
		SceneRender* SceneRenderRaw = nullptr;
		float EnvironmentRotatePitchDegrees = 0.f;
		float EnvironmentRotateYawDegrees = 1.f;
	};
}
