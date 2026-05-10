#include "Render/SkyLightEnvironment.h"
#include "Render/SkyLightEnvironmentRDGPasses.h"
#include "Render/SkyLightEnvironmentPrecomputeState.h"
#include "Render/RDGBuilder.h"
#include "RHI/RHICommandContext.h"

namespace Engine
{
	void FSkyLightEnvironmentRDGPasses::RegisterPasses(USkyLightComponent& Skylight, FRDGBuilder& Graph,
														 RenderCore::RHICommandContext& RHIContext)
	{
		FSkyLightEnvironmentPrecomputeState* d = Skylight.d_ptr;
		if (!d)
			return;
		if (d->Host.bInitRender)
			return;
		if (!d->Host.bProceduralSkyActive && !d->SpecifiedCubemap.HDRTex)
			return;

		Graph.AddPass(FRDGPassDescriptor{
			"SkyLight_CaptureCubemap",
			{ { "SkyLight_SourceHDR",
				[HDR = d->SpecifiedCubemap.HDRTex]() { return HDR; },
				false,
				FRDGResourceAccess::SRV } },
			{ { "SkyLight_Cubemap", []() { return std::shared_ptr<RenderCore::RHITexture2D>{}; }, false, FRDGResourceAccess::RTV } },
			[&Skylight, &RHIContext]() { Skylight.CaptureSkyLightCubemap(RHIContext); },
			false,
			RDG_Raster | RDG_MayCullIfUnreachableFromSink,
			ERDGPassQueue::Graphics });

		Graph.AddPass(FRDGPassDescriptor{
			"SkyLight_GenerateDiffuseIrradiance",
			{ { "SkyLight_Cubemap", []() { return std::shared_ptr<RenderCore::RHITexture2D>{}; }, false, FRDGResourceAccess::SRV } },
			{ { "SkyLight_DiffuseIrradiance", []() { return std::shared_ptr<RenderCore::RHITexture2D>{}; }, false, FRDGResourceAccess::RTV } },
			[&Skylight, &RHIContext]() { Skylight.GenerateDiffuseIrradiance(RHIContext); },
			false,
			RDG_Raster | RDG_MayCullIfUnreachableFromSink,
			ERDGPassQueue::Graphics });

		Graph.AddPass(FRDGPassDescriptor{
			"SkyLight_GenerateSpecularPrefilter",
			{ { "SkyLight_Cubemap", []() { return std::shared_ptr<RenderCore::RHITexture2D>{}; }, false, FRDGResourceAccess::SRV } },
			{ { "SkyLight_SpecularPrefilter", []() { return std::shared_ptr<RenderCore::RHITexture2D>{}; }, false, FRDGResourceAccess::RTV } },
			[&Skylight, &RHIContext]() { Skylight.GenerateSpecularPrefilter(RHIContext); },
			false,
			RDG_Raster | RDG_MayCullIfUnreachableFromSink,
			ERDGPassQueue::Graphics });

		Graph.AddPass(FRDGPassDescriptor{
			"SkyLight_Finalize",
			{ { "SkyLight_SpecularPrefilter", []() { return std::shared_ptr<RenderCore::RHITexture2D>{}; }, false, FRDGResourceAccess::SRV } },
			{},
			[d]() { d->Host.bInitRender = true; },
			false,
			RDG_GraphSink,
			ERDGPassQueue::Graphics });
	}

} // namespace Engine
