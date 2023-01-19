#pragma once
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Component.h"

namespace Engine 
{
	struct ActorP
	{
		// Actor's state
		Actor::State State = Actor::State::EActive;

		// Transform
		math::Vector3 Position;
		float Scale = 1.0f;
		math::Quaternion Rotation;

		std::vector<std::shared_ptr<Component>> Components;
		std::weak_ptr<Scene> SceneMgr;

		bool RecomputeWorldTransform = true;

		math::Matrix4x4 WorldTransform;
	};
}