#pragma once
#include "Render/SceneRendering/DeferredBasePassDrawContext.h"

namespace Engine
{
	/** Records opaque and translucent mesh draws into the deferred scene texture targets. */
	class FDeferredShadingBasePassRenderer
	{
	public:
		static void RenderBasePassOpaque(const FDeferredBasePassDrawContext& DrawContext);
		static void RenderBasePassTranslucent(const FDeferredBasePassDrawContext& DrawContext);
		static void RenderDeferredBasePassFullSequence(const FDeferredBasePassDrawContext& DrawContext);

		/** Forward translucent PBR onto lit SceneColor (after deferred lighting). */
		static void RenderTranslucentForward(const FDeferredBasePassDrawContext& DrawContext);

		/** Forward fur shells onto lit SceneColor (after deferred lighting). */
		static void RenderFurForward(const FDeferredBasePassDrawContext& DrawContext);
	};
}
