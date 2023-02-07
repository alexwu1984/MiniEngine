#include "Scene/Actor.h"
#include "Scene/ActorPrivate.h"
#include "Scene/Component.h"

namespace Engine
{

	IMP_ACTOR_CLASS_NAME(Actor)
	IMP_ACTOR_TRAITS_CLASS_NAME(Actor)

	Actor::Actor(std::weak_ptr<SceneView> Scene)
		:ImplActorP(std::make_shared<ActorP>())
	{
		ImplActorP->Scene = Scene;
	}
	Actor::~Actor()
	{

	}

	void Actor::InitResouce()
	{

	}

	void Actor::Tick(float deltaTime)
	{
		if (ImplActorP->State == AState::EActive)
		{
			ComputeWorldTransform();

			TickComponents(deltaTime);
			TickActor(deltaTime);
		}
	}

	void Actor::TickComponents(float deltaTime)
	{
		for (auto comp : ImplActorP->Components)
		{
			comp->Tick(deltaTime);
		}
	}

	void Actor::TickActor(float deltaTime)
	{

	}

	Vector3 Actor::GetPosition() const
	{
		return ImplActorP->Position;
	}

	void Actor::SetPosition(const Vector3& pos)
	{
		ImplActorP->Position = pos;
		ImplActorP->RecomputeWorldTransform = true;
	}

	float Actor::GetScale() const
	{
		return ImplActorP->Scale;
	}

	void Actor::SetScale(float scale)
	{
		ImplActorP->Scale = scale;
		ImplActorP->RecomputeWorldTransform = true;
	}

	math::Quaternion Actor::GetRotation() const
	{
		return ImplActorP->Rotation;
	}

	void Actor::SetRotation(const Quaternion& rotation)
	{
		ImplActorP->Rotation = rotation;
		ImplActorP->RecomputeWorldTransform = true;
	}

	void Actor::ComputeWorldTransform()
	{
		if (ImplActorP->RecomputeWorldTransform)
		{
			ImplActorP->RecomputeWorldTransform = false;
			// Scale, then rotate, then translate
			ImplActorP->WorldTransform = Matrix4x4::ScaleMatrix(ImplActorP->Scale);
			ImplActorP->WorldTransform *= Matrix4x4::CreateFromQuaternion(ImplActorP->Rotation);
			ImplActorP->WorldTransform *= Matrix4x4::CreateFromTranslate(ImplActorP->Position);

			// Inform components world transform updated
			for (auto comp : ImplActorP->Components)
			{
				comp->OnUpdateWorldTransform();
			}
		}
	}

	const Matrix4x4& Actor::GetWorldTransform() const
	{
		return ImplActorP->WorldTransform;
	}

	Actor::AState Actor::GetState() const
	{
		return ImplActorP->State;
	}

	void Actor::SetState(AState State)
	{
		ImplActorP->Scale = State;
	}

	std::shared_ptr<SceneView> Actor::GetScene() const
	{
		return ImplActorP->Scene.lock();
	}

	Vector3 Actor::GetForward() const
	{
		return Vector3::Transform(Vector3::UnitX, ImplActorP->Rotation);
	}

	Vector3 Actor::GetRight() const
	{
		return Vector3::Transform(Vector3::UnitY, ImplActorP->Rotation);
	}

	Vector3 Actor::GetUp() const
	{
		return Vector3::Transform(Vector3::UnitZ, ImplActorP->Rotation);
	}

	void Actor::RotateToNewForward(const math::Vector3& Forward)
	{
		// Figure out difference between original (unit x) and new
		float dot = Vector3::Dot(Vector3::UnitX, Forward);
		float angle = std::acosf(dot);
		// Facing down X
		if (dot > 0.9999f)
		{
			SetRotation(Quaternion::Identity);
		}
		// Facing down -X
		else if (dot < -0.9999f)
		{
			SetRotation(Quaternion(Vector3::UnitZ, MATH_PI));
		}
		else
		{
			// Rotate about axis from cross product
			Vector3 axis = Vector3::Cross(Vector3::UnitX, Forward);
			axis.Normalize();
			SetRotation(Quaternion(axis, angle));
		}
	}

	void Actor::AddComponent(std::shared_ptr<Component> component)
	{
		ImplActorP->Components.push_back(component);
	
	}

	void Actor::RemoveComponent(std::shared_ptr<Component> component)
	{
		auto iter = std::find(ImplActorP->Components.begin(), ImplActorP->Components.end(), component);
		if (iter != ImplActorP->Components.end())
		{
			ImplActorP->Components.erase(iter);
		}
	}

	std::vector<std::shared_ptr<Engine::Component>>& Actor::GetComponents() const
	{
		return ImplActorP->Components;
	}

	void Actor::ProcessInput(const InputDeviceState& State)
	{
		for (auto comp : ImplActorP->Components)
		{
			comp->ProcessInput(State);
		}
	}

}