#pragma once
#include "Scene/Actor.h"

namespace Engine
{
	struct GltfActorP;

	class GltfActor : public Actor
	{
	public:
		GltfActor(std::weak_ptr<SceneView> Scene,const std::wstring& FileName);
		virtual ~GltfActor();

		virtual void InitResouce() override;

	private:
		std::shared_ptr<GltfActorP> Impl;
	};
}