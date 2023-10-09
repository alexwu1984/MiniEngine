#pragma once
#include "core/inc.h"
#include "core/event.h"

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

	class SceneRender
	{
	public:
		SceneRender(std::weak_ptr<SceneView> Owner);
		~SceneRender();
		std::shared_ptr<SceneView> GetOwner() const;

		void InitResource(std::shared_ptr<RenderCore::RHIViewPort> ViewPort);
		void LoadConfig(const std::wstring& FileName);
		void Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen);
		void Render();
		std::shared_ptr<PreProcessor> GetPreProcessor();

	private:
		SceneRenderPrivate* d_ptr = nullptr;
	};
}