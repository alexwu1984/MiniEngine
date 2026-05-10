#pragma once
#include "Render/SkyLightEnvironment.h"
#include "Render/SkyLightEnvironmentBakePipeline.h"
#include "Render/SkyLightEnvironmentSources.h"
#include "RHI/RHITexture2D.h"
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
	/** Relative to GLTFModel/ (UTF-8). Lower-hemisphere IBL lat-long when sky is procedural; empty = disabled. */
	std::string ConfigGroundIBLHdrUtf8{};
	std::string CurrentGroundIBLHdrUtf8{};
	float ConfigGroundIBLIntensity = 1.f;
	float CurrentGroundIBLIntensity = 1.f;
	float ConfigHemiIBLBlendPower = 1.75f;
	float CurrentHemiIBLBlendPower = 1.75f;
	std::shared_ptr<RenderCore::RHITexture2D> GroundHemiLatLongTex{};
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
