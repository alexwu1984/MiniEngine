#pragma once
#include "core/inc.h"
#include "core/event.h"
#include "core/color.h"

namespace RenderCore
{
	class RHICommandContext;
	class RHIViewPort;
}

namespace Engine
{
	class PreProcessor;
	class SceneView;
	struct SceneRenderPrivate;
	class ShadowRenderPass;
	class SimplePostProcessor;
	class CubeBackground;

	class SceneRender
	{
	public:
		SceneRender(std::weak_ptr<SceneView> Owner);
		~SceneRender();
		std::shared_ptr<SceneView> GetOwner() const;

		void InitResource(std::shared_ptr<RenderCore::RHIViewPort> ViewPort);
		void LoadConfig(const std::wstring& FileName);
		void SetBackgroundColor(const core::FLinearColor& Color);
		void Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen);
		void Render(float DeltaTime);
		void SetIBLRotate(float x, float y);
		std::shared_ptr<PreProcessor> GetPreProcessor() const;
		std::shared_ptr<ShadowRenderPass>  GetShadowRenderPass() const;
		std::shared_ptr<RenderCore::RHIViewPort> GetViewPort() const;

		void SetSamplePostProcessor(std::shared_ptr<SimplePostProcessor> postProcessor);
	public:
		core::event<void()> sigGuiEvent;
	private:
		SceneRenderPrivate* d_ptr = nullptr;
	};
}