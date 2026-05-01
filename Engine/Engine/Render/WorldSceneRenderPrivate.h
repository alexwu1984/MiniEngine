#pragma once
#include "core/color.h"
#include "Render/RDGBuilder.h"
#include "Render/SceneRendering/SceneRenderer.h"
#include <atomic>
#include <memory>
#include <mutex>

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
	class DeferredLightingPass;

	struct FWorldSceneRenderPrivate
	{
		std::weak_ptr<World> Owner;
		std::shared_ptr<RenderCore::RHIViewPort> MainViewPort;
		std::shared_ptr<PreProcessor> PreProcess;
		std::shared_ptr<PostProcessor> PostProcess;
		std::unique_ptr<FMeshMaterialRenderCache> MeshMaterialRenderCache;
		std::shared_ptr<CubeBackground> BackgroundRender;
		std::shared_ptr<GBuffer> TargetBuffer;
		std::shared_ptr<ShadowRenderPass> ShadowRender;
		std::shared_ptr<DeferredLightingPass> DeferredLighting;
		std::atomic_bool IsInit{ false };
		core::FLinearColor Color = core::FLinearColor::Blue;
		FRDGCompileParameters RDGCompileParams{};
		float DeferredBasePassEnvironmentRotateX = 0.f;
		float DeferredBasePassEnvironmentRotateY = 1.f;

		bool bUnlit = false;

		bool bEnableDeferredLightingPass = false;

		FSceneRenderer SceneRenderer;
		std::mutex RenderFrameMutex;
	};
} // namespace Engine
