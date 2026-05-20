#pragma once
#include "core/inc.h"
#include "Render/SceneRendering/SceneViewData.h"

namespace RenderCore
{
	class DynamicRHI;
	class RHICommandContext;
}

namespace Engine
{
	class FSceneTextures;
	struct FGBufferVisualizationPassPrivate;

	/** UE-style buffer visualization modes (fullscreen replace SceneColor before post-process). */
	enum class EGBufferVisualizeMode : int32_t
	{
		None = 0,
		BaseColor,
		Normal,
		Metallic,
		Roughness,
		AmbientOcclusion,
		Emissive,
		Depth,
		MaterialAux,
		LitSceneColor,
	};

	struct FGBufferVisualizationSettings
	{
		EGBufferVisualizeMode Mode = EGBufferVisualizeMode::None;
	};

	class FGBufferVisualizationPass
	{
	public:
		explicit FGBufferVisualizationPass(RenderCore::DynamicRHI* InRHI);
		~FGBufferVisualizationPass();

		void InitResource();
		void Execute(RenderCore::RHICommandContext& RHIContext, const std::shared_ptr<FSceneTextures>& SceneTextures,
					 const FSceneViewData& ViewData, EGBufferVisualizeMode Mode) const;

	private:
		FGBufferVisualizationPassPrivate* d_ptr = nullptr;
	};
}
