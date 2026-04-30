#pragma once
#include "core/color.h"
#include "Render/FrameGraph.h"
#include "Render/SceneRendering/FSceneRenderer.h"
#include <atomic>
#include <memory>

namespace RenderCore
{
	class RHIViewPort;
}

namespace Engine
{
	class World;
	class PreProcessor;
	class PostProcessor;
	class CubeBackground;
	class GBuffer;
	class ShadowRenderPass;
	class FMeshMaterialRenderCache;

	struct SceneRenderPrivate
	{
		std::weak_ptr<World> Owner;
		std::shared_ptr<RenderCore::RHIViewPort> MainViewPort;
		std::shared_ptr<PreProcessor> PreProcess;
		std::shared_ptr<PostProcessor> PostProcess;
		std::unique_ptr<FMeshMaterialRenderCache> MeshMaterialRenderCache;
		std::shared_ptr<CubeBackground> BackgroundRender;
		std::shared_ptr<GBuffer> TargetBuffer;
		std::shared_ptr<ShadowRenderPass> ShadowRender;
		std::atomic_bool IsInit{ false };
		core::FLinearColor Color = core::FLinearColor::Blue;
		FrameGraphCompileParams RDGCompileParams{};
		float DeferredBasePassEnvironmentRotateX = 0.f;
		float DeferredBasePassEnvironmentRotateY = 1.f;

		bool bUnlit = false;

		FSceneRenderer SceneFrameRenderer;
	};
}
