#include "Render/SceneRender.h"
#include "Scene/SceneView.h"

namespace Engine
{
	struct SceneRenderP
	{
		std::weak_ptr<SceneView> Owner;

	};
	
	SceneRender::SceneRender(std::weak_ptr<SceneView> Owner)
		:Impl(std::make_shared<SceneRenderP>())
	{
		Impl->Owner = Owner;
	}

	SceneRender::~SceneRender()
	{

	}

	std::shared_ptr<SceneView> SceneRender::GetOwner() const
	{
		return Impl->Owner.lock();
	}

	void SceneRender::InitResource()
	{

	}

}


