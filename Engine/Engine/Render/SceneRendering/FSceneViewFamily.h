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
		bool bUsesTemporalAAProjectionJitter = false;
		std::vector<FSceneViewData> Views;

		FSceneViewData& PrimaryView();
		const FSceneViewData& PrimaryView() const;
	};
}
