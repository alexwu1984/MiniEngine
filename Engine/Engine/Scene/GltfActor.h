#pragma once
#include "Scene/Actor.h"
#include "json.h"

namespace Engine
{
	struct GltfActorP;

	class GltfActor final: public Actor
	{
	public:
		DECLARE_ACTOR_CLASS_NAME(GltfActor)
		GltfActor(std::weak_ptr<SceneView> Scene, const nlohmann::json& GltfJson);
		virtual ~GltfActor();

		void InitResouce() override;

	private:
		std::shared_ptr< GltfActorP> Impl;
	};

	DECLARE_ACTOR_TRAITS_CLASS_NAME(GltfActor)
}