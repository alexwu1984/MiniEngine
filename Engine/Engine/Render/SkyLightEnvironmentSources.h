#pragma once
#include "Render/SkyLightEnvironmentBakePipeline.h"
#include "RHI/RHIUniformBuffer.h"

namespace Engine
{
/** Lat-long HDR file → EvnCube (IBLLongLatToCube). */
struct FSpecifiedCubemapEnvironmentSource
{
	std::shared_ptr<RenderCore::RHITexture2D> HDRTex;
	void CaptureRadianceCubemap(RenderCore::RHICommandContext& RHIContext, FSkyLightEnvironmentBakePipeline& Bake);
};

/** SkyAtmosphere.hlsl (entry PS_ProceduralSkyCube) → EvnCube. */
struct FProceduralSkyEnvironmentSource
{
	std::shared_ptr<RenderCore::RHIPixelShader> PSProceduralSkyCube;
	std::shared_ptr<RenderCore::RHIUniformBuffer> ProceduralSkyPSCB;
	float ProceduralSunDirX = 1.f;
	float ProceduralSunDirY = 0.05f;
	float ProceduralSunDirZ = 0.f;

	void InitCubemapPixelShader(RenderCore::DynamicRHI* RHI, const std::wstring& ShaderLibDirectory);
	void CaptureRadianceCubemap(RenderCore::RHICommandContext& RHIContext, FSkyLightEnvironmentBakePipeline& Bake);
};

} // namespace Engine
