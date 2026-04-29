#pragma once
#include <memory>

namespace Engine
{
	class GBuffer;
	class SceneRender;
	struct FSceneViewData;

	/** View and scene bindings consumed while recording deferred base pass draws. */
	struct FDeferredBasePassDrawContext
	{
		std::shared_ptr<const FSceneViewData> ViewData;
		std::shared_ptr<GBuffer> TargetBuffer;
		SceneRender* SceneRenderRaw = nullptr;
	};
}
