#pragma once
#include "Scene/Actor.h"
#include "json.h"

namespace Engine
{
	struct GltfActorPrivate;

	class GltfActor final: public Actor
	{
	public:
		DECLARE_ACTOR_CLASS_NAME(GltfActor)
		GltfActor(std::weak_ptr<World> InWorld, const nlohmann::json& GltfJson);
		virtual ~GltfActor();

		void InitResouce() override;

	private:
		GltfActorPrivate *d_ptr = nullptr;
	};

	DECLARE_ACTOR_TRAITS_CLASS_NAME(GltfActor)
}