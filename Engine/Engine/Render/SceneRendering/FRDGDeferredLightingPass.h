#pragma once
#include "Render/FRDGBuilder.h"
#include <memory>
#include <vector>

namespace Engine
{
	class GBuffer;

	/** Deferred lighting pass: SceneColor pre-lighting copy import plus GatherPassInputs/Outputs for scheduling and access metadata. */
	struct FRDGDeferredLightingPass
	{
		static constexpr const char* PassName = "DeferredLighting";
		/** RDG name for GBuffer::GetSceneColorPreLighting() (copy of SceneColor before fullscreen lighting). */
		static constexpr const char* TextureNameSceneColorPreLighting = "SceneColorPreLighting";

		/** Register pool-owned pre-lighting texture so inputs can be Required without spurious compile warnings. */
		static void RegisterExternalImports(FRDGBuilder& Graph, const std::shared_ptr<GBuffer>& GBuffer);

		static std::vector<FRDGPassResource> GatherPassInputs(const std::shared_ptr<GBuffer>& GBuffer);
		static std::vector<FRDGPassResource> GatherPassOutputs(const std::shared_ptr<GBuffer>& GBuffer);
	};
} // namespace Engine
