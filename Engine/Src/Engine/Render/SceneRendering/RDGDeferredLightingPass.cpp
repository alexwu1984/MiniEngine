#include "Render/SceneRendering/RDGDeferredLightingPass.h"
#include "Render/GBuffer.h"

namespace Engine
{
	std::vector<FRDGPassResource> FRDGDeferredLightingPass::GatherCopyPassInputs(const std::shared_ptr<GBuffer>& GBuffer)
	{
		if (!GBuffer)
			return {};
		return {
			{ "SceneColor", [GBuffer]() { return GBuffer->GetSceneColor(); }, true, FRDGResourceAccess::CopySrc },
		};
	}

	std::vector<FRDGPassResource> FRDGDeferredLightingPass::GatherCopyPassOutputs(const std::shared_ptr<GBuffer>& GBuffer)
	{
		if (!GBuffer)
			return {};
		return {
			{ TextureNameSceneColorPreLighting, [GBuffer]() { return GBuffer->GetSceneColorPreLighting(); }, true, FRDGResourceAccess::CopyDst },
		};
	}

	std::vector<FRDGPassResource> FRDGDeferredLightingPass::GatherRasterPassInputs(const std::shared_ptr<GBuffer>& GBuffer)
	{
		if (!GBuffer)
			return {};
		using A = FRDGResourceAccess;
		return {
			{ TextureNameSceneColorPreLighting, [GBuffer]() { return GBuffer->GetSceneColorPreLighting(); }, true, A::SRV },
			{ "Normal", [GBuffer]() { return GBuffer->GetNormalBuffer(); }, true, A::SRV },
			{ "Emissive", [GBuffer]() { return GBuffer->GetEmissiveBuffer(); }, true, A::SRV },
			{ "MetallicRoughness", [GBuffer]() { return GBuffer->GetMetallicRoughnessBuffer(); }, true, A::SRV },
			{ "Depth", [GBuffer]() { return GBuffer->GetDepth(); }, true, A::SRV },
		};
	}

	std::vector<FRDGPassResource> FRDGDeferredLightingPass::GatherRasterPassOutputs(const std::shared_ptr<GBuffer>& GBuffer)
	{
		if (!GBuffer)
			return {};
		return {
			{ "SceneColor", [GBuffer]() { return GBuffer->GetSceneColor(); }, true, FRDGResourceAccess::RTV },
		};
	}
} // namespace Engine
