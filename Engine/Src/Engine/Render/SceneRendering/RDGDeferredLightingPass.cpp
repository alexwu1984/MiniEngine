#include "Render/SceneRendering/RDGDeferredLightingPass.h"
#include "Render/GBuffer.h"

namespace Engine
{
	void FRDGDeferredLightingPass::RegisterExternalImports(FRDGBuilder& Graph, const std::shared_ptr<GBuffer>& GBuffer)
	{
		if (!GBuffer)
			return;
		Graph.ImportTexture(TextureNameSceneColorPreLighting, [GBuffer]() { return GBuffer->GetSceneColorPreLighting(); }, false);
	}

	std::vector<FRDGPassResource> FRDGDeferredLightingPass::GatherPassInputs(const std::shared_ptr<GBuffer>& GBuffer)
	{
		if (!GBuffer)
			return {};
		using A = FRDGResourceAccess;
		return {
			{ "SceneColor", [GBuffer]() { return GBuffer->GetSceneColor(); }, true, A::CopySrc },
			{ TextureNameSceneColorPreLighting, [GBuffer]() { return GBuffer->GetSceneColorPreLighting(); }, true, A::CopyDst },
			{ "Normal", [GBuffer]() { return GBuffer->GetNormalBuffer(); }, true, A::SRV },
			{ "Emissive", [GBuffer]() { return GBuffer->GetEmissiveBuffer(); }, true, A::SRV },
			{ "MetallicRoughness", [GBuffer]() { return GBuffer->GetMetallicRoughnessBuffer(); }, true, A::SRV },
			{ "Depth", [GBuffer]() { return GBuffer->GetDepth(); }, true, A::SRV },
		};
	}

	std::vector<FRDGPassResource> FRDGDeferredLightingPass::GatherPassOutputs(const std::shared_ptr<GBuffer>& GBuffer)
	{
		if (!GBuffer)
			return {};
		return {
			{ "SceneColor", [GBuffer]() { return GBuffer->GetSceneColor(); }, true, FRDGResourceAccess::RTV },
		};
	}
} // namespace Engine
