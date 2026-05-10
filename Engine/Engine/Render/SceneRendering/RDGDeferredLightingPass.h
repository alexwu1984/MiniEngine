#pragma once
#include "Render/RDGBuilder.h"
#include <memory>
#include <vector>

namespace Engine
{
	class FSceneTextures;

	/**
	 * Deferred lighting in two RDG passes so scheduling matches GPU semantics:
	 * (1) copy base-pass SceneColor -> SceneColorPreLighting, (2) fullscreen raster writes lit HDR to SceneColor.
	 * Separating avoids folding CopySrc + RTV on SceneColor into one pass node (ordering/metadata only; barriers still RHI-side).
	 */
	struct FRDGDeferredLightingPass
	{
		static constexpr const char* PassNameCopySceneToPreLighting = "DeferredLighting_CopySceneColor";
		static constexpr const char* PassNameRaster = "DeferredLighting";
		/** RDG name for FSceneTextures::GetSceneColorPreLighting() (copy of SceneColor before fullscreen lighting). */
		static constexpr const char* TextureNameSceneColorPreLighting = "SceneColorPreLighting";

		static std::vector<FRDGPassResource> GatherCopyPassInputs(const std::shared_ptr<FSceneTextures>& SceneTextures);
		static std::vector<FRDGPassResource> GatherCopyPassOutputs(const std::shared_ptr<FSceneTextures>& SceneTextures);

		static std::vector<FRDGPassResource> GatherRasterPassInputs(const std::shared_ptr<FSceneTextures>& SceneTextures);
		static std::vector<FRDGPassResource> GatherRasterPassOutputs(const std::shared_ptr<FSceneTextures>& SceneTextures);
	};
} // namespace Engine
