#pragma once
#include "Render/SceneRendering/DeferredBasePassDrawContext.h"

namespace Engine
{
	/** Issues opaque mesh draws for the deferred base pass in approximate front-to-back order per actor. */
	class FOpaqueMeshDrawBuilder
	{
	public:
		static void DrawSortedOpaqueMeshes(const FDeferredBasePassDrawContext& DrawContext, bool bIsPrePass);
	};
}
