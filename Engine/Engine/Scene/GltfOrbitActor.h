#pragma once
#include "Scene/Actor.h"

namespace Engine
{
	struct GltfOrbitActorP;

	class GltfOrbitActor final: public Actor
	{
	public:
		DECLARE_ACTOR_CLASS_NAME(GltfOrbitActor)
		GltfOrbitActor(std::weak_ptr<SceneView> Scene,const std::wstring& FileName);
		virtual ~GltfOrbitActor();

		void InitResouce() override;

	private:
		std::shared_ptr< GltfOrbitActorP> Impl;
	};

	DECLARE_ACTOR_TRAITS_CLASS_NAME(GltfOrbitActor)
}