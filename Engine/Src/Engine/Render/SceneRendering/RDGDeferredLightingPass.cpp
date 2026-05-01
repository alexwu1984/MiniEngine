#include "Render/SceneRendering/RDGDeferredLightingPass.h"
#include "Render/SceneTextures.h"

namespace Engine
{
	std::vector<FRDGPassResource> FRDGDeferredLightingPass::GatherCopyPassInputs(const std::shared_ptr<SceneTextures>& TargetTextures)
	{
		if (!TargetTextures)
			return {};
		return {
			{ "SceneColor", [TargetTextures]() { return TargetTextures->GetSceneColor(); }, true, FRDGResourceAccess::CopySrc },
		};
	}

	std::vector<FRDGPassResource> FRDGDeferredLightingPass::GatherCopyPassOutputs(const std::shared_ptr<SceneTextures>& TargetTextures)
	{
		if (!TargetTextures)
			return {};
		return {
			{ TextureNameSceneColorPreLighting, [TargetTextures]() { return TargetTextures->GetSceneColorPreLighting(); }, true, FRDGResourceAccess::CopyDst },
		};
	}

	std::vector<FRDGPassResource> FRDGDeferredLightingPass::GatherRasterPassInputs(const std::shared_ptr<SceneTextures>& TargetTextures)
	{
		if (!TargetTextures)
			return {};
		using A = FRDGResourceAccess;
		return {
			{ TextureNameSceneColorPreLighting, [TargetTextures]() { return TargetTextures->GetSceneColorPreLighting(); }, true, A::SRV },
			{ "Normal", [TargetTextures]() { return TargetTextures->GetNormalBuffer(); }, true, A::SRV },
			{ "Emissive", [TargetTextures]() { return TargetTextures->GetEmissiveBuffer(); }, true, A::SRV },
			{ "MetallicRoughness", [TargetTextures]() { return TargetTextures->GetMetallicRoughnessBuffer(); }, true, A::SRV },
			{ "Depth", [TargetTextures]() { return TargetTextures->GetDepth(); }, true, A::SRV },
		};
	}

	std::vector<FRDGPassResource> FRDGDeferredLightingPass::GatherRasterPassOutputs(const std::shared_ptr<SceneTextures>& TargetTextures)
	{
		if (!TargetTextures)
			return {};
		return {
			{ "SceneColor", [TargetTextures]() { return TargetTextures->GetSceneColor(); }, true, FRDGResourceAccess::RTV },
		};
	}
} // namespace Engine
