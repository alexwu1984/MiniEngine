#include "Render/SkyLightEnvironment.h"
#include "Render/SkyLightIBLPrecomputePrivate.h"
#include "Render/RDGBuilder.h"
#include "RHI/RHICommandContext.h"

namespace Engine
{
	void FSkyLightIBLPrecompute::AddFramePasses(FRDGBuilder& Graph, RenderCore::RHICommandContext& RHIContext)
	{
		C_P(FSkyLightIBLPrecompute);
		if (d->bInitRender)
			return;
		if (!d->bProceduralSkyActive && !d->HDRTex)
			return;

		Graph.AddPass(FRDGPassDescriptor{
			"SkyLight_CaptureCubemap",
			{ { "SkyLight_SourceHDR", [HDR = d->HDRTex]() { return HDR; }, false, FRDGResourceAccess::SRV } },
			{ { "SkyLight_Cubemap", []() { return std::shared_ptr<RenderCore::RHITexture2D>{}; }, false, FRDGResourceAccess::RTV } },
			[this, &RHIContext]() { CaptureSkyLightCubemap(RHIContext); },
			false,
			RDG_Raster | RDG_MayCullIfUnreachableFromSink,
			ERDGPassQueue::Graphics });

		Graph.AddPass(FRDGPassDescriptor{
			"SkyLight_GenerateDiffuseIrradiance",
			{ { "SkyLight_Cubemap", []() { return std::shared_ptr<RenderCore::RHITexture2D>{}; }, false, FRDGResourceAccess::SRV } },
			{ { "SkyLight_DiffuseIrradiance", []() { return std::shared_ptr<RenderCore::RHITexture2D>{}; }, false, FRDGResourceAccess::RTV } },
			[this, &RHIContext]() { GenerateDiffuseIrradiance(RHIContext); },
			false,
			RDG_Raster | RDG_MayCullIfUnreachableFromSink,
			ERDGPassQueue::Graphics });

		Graph.AddPass(FRDGPassDescriptor{
			"SkyLight_GenerateSpecularPrefilter",
			{ { "SkyLight_Cubemap", []() { return std::shared_ptr<RenderCore::RHITexture2D>{}; }, false, FRDGResourceAccess::SRV } },
			{ { "SkyLight_SpecularPrefilter", []() { return std::shared_ptr<RenderCore::RHITexture2D>{}; }, false, FRDGResourceAccess::RTV } },
			[this, &RHIContext]() { GenerateSpecularPrefilter(RHIContext); },
			false,
			RDG_Raster | RDG_MayCullIfUnreachableFromSink,
			ERDGPassQueue::Graphics });

		Graph.AddPass(FRDGPassDescriptor{
			"SkyLight_Finalize",
			{ { "SkyLight_SpecularPrefilter", []() { return std::shared_ptr<RenderCore::RHITexture2D>{}; }, false, FRDGResourceAccess::SRV } },
			{},
			[this]() {
				C_P(FSkyLightIBLPrecompute);
				d->bInitRender = true;
			},
			false,
			RDG_GraphSink,
			ERDGPassQueue::Graphics });
	}

} // namespace Engine
