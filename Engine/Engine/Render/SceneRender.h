#pragma once
#include "core/inc.h"

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

		void InitResource();

	private:
		std::shared_ptr< SceneRenderP> Impl;
	};
}