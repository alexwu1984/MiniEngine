#pragma once
#include "Engine/Scene/SceneView.h"
#include "Engine/Scene/Component.h"

namespace Engine 
{
	using namespace math;

	struct ActorPrivate
	{
		// Actor's state
		Actor::AState State = Actor::AState::EActive;

		// Transform
		Vector3 Position;
		float Scale = 1.0f;
		Quaternion Rotation;

		std::vector<std::shared_ptr<Component>> Components;
		std::weak_ptr<SceneView> Scene;

		bool RecomputeWorldTransform = true;
		bool projectShadow = false;
		bool visible = true;

		Matrix4x4 PrevWorldTransform;
		Matrix4x4 WorldTransform;

		std::wstring ActorName;
	};
}