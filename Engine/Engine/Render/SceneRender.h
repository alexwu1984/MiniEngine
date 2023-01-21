#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class RHIViewPort;
}

namespace Engine
{
	
	class SceneView;
	struct SceneRenderP;

	class SceneRender
	{
	public:
		SceneRender(std::weak_ptr<SceneView> Owner);
		~SceneRender();
		std::shared_ptr<SceneView> GetOwner() const;

		void InitResource(std::shared_ptr<RenderCore::RHIViewPort> ViewPort);

		void Render();

	private:
		std::shared_ptr< SceneRenderP> Impl;
	};
}