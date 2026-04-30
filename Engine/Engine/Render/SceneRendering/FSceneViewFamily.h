#pragma once
#include "Render/SceneRendering/FSceneViewData.h"
#include <cstdint>
#include <vector>

namespace Engine
{
	/** View family: shared resolution / flags for one or more views (UE FSceneViewFamily shell). */
	struct FSceneViewFamily
	{
		uint32_t RenderSizeX = 0;
		uint32_t RenderSizeY = 0;
		/** Mirrors main view: Halton sub-pixel jitter in VP when post AA requests it (see PostProcessor::WantsHaltonProjectionJitterForMainPass). */
		bool bHaltonProjectionJitterForMainPass = false;
		std::vector<FSceneViewData> Views;

		FSceneViewData& PrimaryView();
		const FSceneViewData& PrimaryView() const;
	};
}
