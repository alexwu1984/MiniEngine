#pragma once
#include "Engine/Scene/World.h"
#include "Engine/Scene/Component.h"

namespace Engine 
{
	using namespace math;

	struct ActorPrivate
	{
		uint64_t StableInstanceId = 0;

		// Actor's state
		Actor::AState State = Actor::AState::EActive;

		// Transform
		Vector3 Position;
		float Scale = 1.0f;
		Quaternion Rotation;

		std::vector<std::shared_ptr<Component>> Components;
		std::weak_ptr<World> WorldRef;

		bool RecomputeWorldTransform = true;
		bool visible = true;
		/** First Tick: PrevWorldTransform was identity while WorldTransform becomes T → bogus motion/TAA after scene swap. Snap once after publish. */
		bool bMotionPrevInitialized = false;

		Matrix4x4 PrevWorldTransform;
		Matrix4x4 WorldTransform;

		std::wstring ActorName;
	};
}