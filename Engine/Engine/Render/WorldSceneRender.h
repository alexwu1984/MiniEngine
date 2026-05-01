#pragma once
#include "core/inc.h"
#include "core/event.h"
#include "core/color.h"
#include "tinygltf/json.h"
#include <functional>
#include <memory>
#include <string>

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
	class FMeshMaterialRenderCache;

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

	private:
		/** Game thread: gather views/primitives, Submit to FSceneRenderer, enqueue render-thread work. */
		void SubmitSceneForRendering(float DeltaTime);

	public:
		core::event<void()> sigGuiEvent;

	private:
		std::shared_ptr<FWorldSceneRenderPrivate> d_ptr;
	};
} // namespace Engine
