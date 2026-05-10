#pragma once
#include "Render/SkyLightEnvironment.h"
#include "Render/SkyLightEnvironmentBakePipeline.h"
#include "Render/SkyLightEnvironmentSources.h"
#include <mutex>

namespace Engine
{
/** Host-side resolved source + bake completion flag (serialize with render thread). */
struct FSkyLightEnvironmentHostState
{
	mutable std::mutex HdrStateMutex{};
	bool bInitRender = false;
	bool bProceduralSkyActive = false;
	FSkyLightSourceDesc ConfigSource{};
	FSkyLightSourceDesc CurrentSource{};
};

/** Aggregate: shared bake pipeline + pluggable radiance sources + host bookkeeping. */
struct FSkyLightEnvironmentPrecomputeState
{
	FSkyLightEnvironmentBakePipeline Bake;
	FSpecifiedCubemapEnvironmentSource SpecifiedCubemap;
	FProceduralSkyEnvironmentSource ProceduralSky;
	FSkyLightEnvironmentHostState Host;

	explicit FSkyLightEnvironmentPrecomputeState(RenderCore::DynamicRHI* InRHI)
		: Bake(InRHI)
	{
	}
};

#define SKYLIGHT_IBL_DPTR() FSkyLightEnvironmentPrecomputeState* d = d_ptr

} // namespace Engine
