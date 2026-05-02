#pragma once
#include "core/inc.h"
#include "core/event.h"
#include "core/color.h"
#include "tinygltf/json.h"

namespace RenderCore
{
	class DynamicRHI;
	class RHICommandContext;
	class RHIViewPort;
}

namespace Engine
{
	class PreProcessor;
	class PostProcessor;
	class World;
	struct FWorldSceneRenderPrivate;
	class ShadowRenderPass;
	class CubeBackground;

	/**
	 * World-scoped scene rendering entry: viewport, scene textures/post/shadow resources, and game-thread submission.
	 * Paired with FSceneRenderer, which records exactly one submitted frame on the render thread.
	 */
	class FWorldSceneRender : public std::enable_shared_from_this<FWorldSceneRender>
	{
	public:
		FWorldSceneRender(std::weak_ptr<World> Owner);
		~FWorldSceneRender();
		std::shared_ptr<World> GetWorld() const;
		void SetWorldWeak(std::weak_ptr<World> Owner);

		void InitResource(std::shared_ptr<RenderCore::RHIViewPort> ViewPort);
		void LoadConfig(const nlohmann::json& Root);
		void SetBackgroundColor(const core::FLinearColor& Color);
		void Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen);
		void Render(float DeltaTime);
		/** Writes primary SkyLightComponent IBL rotation (degrees); rendering reads it via FSceneViewData each frame. */
		void SetIBLRotate(float x, float y);
		std::shared_ptr<PreProcessor> GetPreProcessor() const;
		std::shared_ptr<PostProcessor> GetPostProcessor() const;
		std::shared_ptr<ShadowRenderPass> GetShadowRenderPass() const;
		std::shared_ptr<RenderCore::RHIViewPort> GetViewPort() const;

		/**
		 * After ReplaceWorld + LoadScene: bump mesh-material scene gen, invalidate mesh cache flag, clear shadow caches;
		 * render thread: InitDefaultSceneTargets + PostProcessor::InvalidateTransientResources (TAA/SSR/Bloom temporals); Flush.
		 * Pair with ApplySceneTransitionPrimaryCameraState() (camera TemporalHistoryGeneration / prev matrices / jitter).
		 */
		void RequestRenderingResetAfterSceneTransition();

		/**
		 * Game thread: enqueue mesh/material cache clear on the render thread and flush.
		 * Call after GPU idle when replacing World — keys use raw pointers that allocators may recycle immediately after teardown.
		 */
		void FlushClearMeshMaterialRenderCacheNow();

		/** Request clearing the mesh draw cache on the next render (cache keys are raw pointers). Actor/resource churn uses this alone. */
		void RequestMeshMaterialRenderCacheInvalidate();

	private:
		/** Game thread: gather views/primitives, Submit to FSceneRenderer, enqueue render-thread work. */
		void SubmitSceneForRendering(float DeltaTime);

	public:
		core::event<void()> sigGuiEvent;

	private:
		std::shared_ptr<FWorldSceneRenderPrivate> d_ptr;
	};
} // namespace Engine
