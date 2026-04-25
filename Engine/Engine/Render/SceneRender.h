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
	class SceneView;
	struct SceneRenderPrivate;
	class ShadowRenderPass;
	class SimplePostProcessor;
	class CubeBackground;

	class SceneRender : public std::enable_shared_from_this<SceneRender>
	{
	public:
		using ExclusiveFullscreenEffectFactory = std::function<std::shared_ptr<SimplePostProcessor>(RenderCore::DynamicRHI*)>;

		SceneRender(std::weak_ptr<SceneView> Owner);
		~SceneRender();
		std::shared_ptr<SceneView> GetOwner() const;

		void InitResource(std::shared_ptr<RenderCore::RHIViewPort> ViewPort);
		void LoadConfig(const nlohmann::json& Root);
		void SetBackgroundColor(const core::FLinearColor& Color);
		void Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen);
		void Render(float DeltaTime);
		void SetIBLRotate(float x, float y);
		std::shared_ptr<PreProcessor> GetPreProcessor() const;
		std::shared_ptr<PostProcessor> GetPostProcessor() const;
		// True when the main post chain uses TAA (geometry must use jittered projection for motion vectors).
		bool UsesTemporalAAProjectionJitter() const;
		std::shared_ptr<ShadowRenderPass>  GetShadowRenderPass() const;
		std::shared_ptr<RenderCore::RHIViewPort> GetViewPort() const;
		void SetSamplePostProcessor(std::shared_ptr<SimplePostProcessor> postProcessor);

		// Register a creatable id for Evn.ExclusiveFullscreenPostEffect in scene JSON (call from viewer/tools, not from Engine).
		static void RegisterExclusiveFullscreenEffect(std::string Id, ExclusiveFullscreenEffectFactory Factory);
		static void UnregisterExclusiveFullscreenEffect(const std::string& Id);
	private:
		void RenderSimple(float DeltaTime);
		void RenderScene(float DeltaTime);
	public:
		core::event<void()> sigGuiEvent;
	private:
		SceneRenderPrivate* d_ptr = nullptr;
	};
}