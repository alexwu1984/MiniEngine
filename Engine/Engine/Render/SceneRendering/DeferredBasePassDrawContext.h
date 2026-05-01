#pragma once
#include <memory>

namespace RenderCore
{
	class RHICommandContext;
}

namespace Engine
{
	class GBuffer;
	class FWorldSceneRender;
	struct FSceneViewData;

	/** View and scene bindings consumed while recording deferred base pass draws. */
	struct FDeferredBasePassDrawContext
	{
		std::shared_ptr<const FSceneViewData> ViewData;
		std::shared_ptr<GBuffer> TargetBuffer;
		FWorldSceneRender* WorldSceneRender = nullptr;
		/** Command list used for the entire frame (D3D12 requires a single consistent recording context per submission). */
		RenderCore::RHICommandContext* RHICmdList = nullptr;
	};
}
