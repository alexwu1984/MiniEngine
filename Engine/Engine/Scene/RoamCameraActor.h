#pragma once
#include "Scene/Actor.h"
#include "json.h"

namespace Engine
{
	class RoamCameraActor final : public Actor
	{
	public:
		DECLARE_ACTOR_CLASS_NAME(RoamCameraActor)
		RoamCameraActor(std::weak_ptr<World> InWorld, const nlohmann::json& RoamJson);
		~RoamCameraActor() override;

		void InitResouce() override;

	private:
		nlohmann::json RoamJson;
	};
	DECLARE_ACTOR_TRAITS_CLASS_NAME(RoamCameraActor);
}
