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
	struct SceneRenderPrivate;
	class ShadowRenderPass;
	class CubeBackground;
	class FMeshMaterialRenderCache;

	class SceneRender : public std::enable_shared_from_this<SceneRender>
	{
	public:
		SceneRender(std::weak_ptr<World> Owner);
		~SceneRender();
		std::shared_ptr<World> GetWorld() const;

		void InitResource(std::shared_ptr<RenderCore::RHIViewPort> ViewPort);
		void LoadConfig(const nlohmann::json& Root);
		void SetBackgroundColor(const core::FLinearColor& Color);
		void Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen);
		void Render(float DeltaTime);
		void SetIBLRotate(float x, float y);
		std::shared_ptr<PreProcessor> GetPreProcessor() const;
		std::shared_ptr<PostProcessor> GetPostProcessor() const;
		std::shared_ptr<ShadowRenderPass> GetShadowRenderPass() const;
		std::shared_ptr<RenderCore::RHIViewPort> GetViewPort() const;

	private:
		void RenderScene(float DeltaTime);

	public:
		core::event<void()> sigGuiEvent;

	private:
		SceneRenderPrivate* d_ptr = nullptr;
	};
}
