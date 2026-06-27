#include "Render/SceneRendering/RDGDeferredLightingPass.h"
#include "Render/SceneTextures.h"

namespace Engine
{
	std::vector<FRDGPassResource> FRDGDeferredLightingPass::GatherCopyPassInputs(const std::shared_ptr<FSceneTextures>& SceneTextures)
	{
		if (!SceneTextures)
			return {};
		return {
			{ "SceneColor", [SceneTextures]() { return SceneTextures->GetSceneColor(); }, true, FRDGResourceAccess::CopySrc },
		};
	}

	std::vector<FRDGPassResource> FRDGDeferredLightingPass::GatherCopyPassOutputs(const std::shared_ptr<FSceneTextures>& SceneTextures)
	{
		if (!SceneTextures)
			return {};
		return {
			{ TextureNameSceneColorPreLighting, [SceneTextures]() { return SceneTextures->GetSceneColorPreLighting(); }, true, FRDGResourceAccess::CopyDst },
		};
	}

	std::vector<FRDGPassResource> FRDGDeferredLightingPass::GatherTransmissionBackgroundCopyInputs(const std::shared_ptr<FSceneTextures>& SceneTextures)
	{
		if (!SceneTextures)
			return {};
		return {
			{ "SceneColor", [SceneTextures]() { return SceneTextures->GetSceneColor(); }, true, FRDGResourceAccess::CopySrc },
		};
	}

	std::vector<FRDGPassResource> FRDGDeferredLightingPass::GatherTransmissionBackgroundCopyOutputs(const std::shared_ptr<FSceneTextures>& SceneTextures)
	{
		if (!SceneTextures)
			return {};
		return {
			{ "SceneColorWithSSR", [SceneTextures]() { return SceneTextures->GetSceneColorWithSSR(); }, true, FRDGResourceAccess::CopyDst },
		};
	}

	std::vector<FRDGPassResource> FRDGDeferredLightingPass::GatherRasterPassInputs(const std::shared_ptr<FSceneTextures>& SceneTextures)
	{
		if (!SceneTextures)
			return {};
		using A = FRDGResourceAccess;
		return {
			{ TextureNameSceneColorPreLighting, [SceneTextures]() { return SceneTextures->GetSceneColorPreLighting(); }, true, A::SRV },
			{ "Normal", [SceneTextures]() { return SceneTextures->GetNormalBuffer(); }, true, A::SRV },
			{ "Emissive", [SceneTextures]() { return SceneTextures->GetEmissiveBuffer(); }, true, A::SRV },
			{ "MetallicRoughness", [SceneTextures]() { return SceneTextures->GetMetallicRoughnessBuffer(); }, true, A::SRV },
			{ "MaterialAux", [SceneTextures]() { return SceneTextures->GetMaterialAuxBuffer(); }, true, A::SRV },
			{ "Depth", [SceneTextures]() { return SceneTextures->GetDepth(); }, true, A::SRV },
		};
	}

	std::vector<FRDGPassResource> FRDGDeferredLightingPass::GatherRasterPassOutputs(const std::shared_ptr<FSceneTextures>& SceneTextures)
	{
		if (!SceneTextures)
			return {};
		return {
			{ "SceneColor", [SceneTextures]() { return SceneTextures->GetSceneColor(); }, true, FRDGResourceAccess::RTV },
		};
	}
} // namespace Engine
