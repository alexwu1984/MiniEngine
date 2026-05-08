#include "core/inc.h"
#include "Scene/Actor.h"
#include "Scene/ActorPrivate.h"
#include "Scene/Component.h"
#include "Scene/World.h"
#include "Scene/FScene.h"
#include "Scene/SceneMeshComponent.h"
#include "Render/RenderStableIds.h"

namespace Engine
{

	IMP_ACTOR_CLASS_NAME(Actor)
	IMP_ACTOR_TRAITS_CLASS_NAME(Actor)

	Actor::Actor(std::weak_ptr<World> InWorld)
		: d_ptr(new ActorPrivate())
	{
		C_P(Actor);
		d->WorldRef = std::move(InWorld);
		d->StableInstanceId = AllocateActorStableInstanceId();
	}
	Actor::~Actor()
	{
		C_P(Actor);
		if (const std::shared_ptr<World> world = d->WorldRef.lock())
		{
			if (const std::shared_ptr<FScene> scene = world->GetScene())
			{
				for (const auto& comp : d->Components)
				{
					if (const auto sm = ComponentCast<SceneMeshComponent>(comp))
						scene->RemoveScenePrimitive(sm);
				}
			}
		}
		delete d_ptr;
	}

	void Actor::InitResouce()
	{

	}

	void Actor::Tick(float deltaTime)
	{
		C_P(Actor);
		if (d->State == AState::EActive)
		{
			ComputeWorldTransform(deltaTime);

			TickComponents(deltaTime);
			TickActor(deltaTime);
		}
	}

	void Actor::TickComponents(float deltaTime)
	{
		C_P(Actor);
		for (auto comp : d->Components)
		{
			comp->Tick(deltaTime);
		}
	}

	void Actor::TickActor(float deltaTime)
	{

	}

	Vector3 Actor::GetPosition() const
	{
		C_P(Actor);
		return d->Position;
	}

	void Actor::SetPosition(const Vector3& pos)
	{
		C_P(Actor);
		d->Position = pos;
		d->RecomputeWorldTransform = true;
	}

	float Actor::GetScale() const
	{
		C_P(Actor);
		return d->Scale;
	}

	void Actor::SetScale(float scale)
	{
		C_P(Actor);
		d->Scale = scale;
		d->RecomputeWorldTransform = true;
	}

	math::Quaternion Actor::GetRotation() const
	{
		C_P(Actor);
		return d->Rotation;
	}

	void Actor::SetRotation(const Quaternion& rotation)
	{
		C_P(Actor);
		d->Rotation = rotation;
		d->RecomputeWorldTransform = true;
	}

	void Actor::ComputeWorldTransform(float deltaTime)
	{
		C_P(Actor);
		d->PrevWorldTransform = d->WorldTransform;

		if (d->RecomputeWorldTransform)
		{
			d->RecomputeWorldTransform = false;

			// Scale, then rotate, then translate
			d->WorldTransform = Matrix4x4::ScaleMatrix(d->Scale);

			d->WorldTransform *= Matrix4x4::CreateFromQuaternion(d->Rotation);
			d->WorldTransform *= Matrix4x4::CreateFromTranslate(d->Position);

			// Inform components world transform updated
			for (auto comp : d->Components)
			{
				comp->OnUpdateWorldTransform(deltaTime);
			}
		}
		if (!d->bMotionPrevInitialized)
		{
			d->PrevWorldTransform = d->WorldTransform;
			d->bMotionPrevInitialized = true;
		}
	}

	const Matrix4x4& Actor::GetWorldTransform() const
	{
		C_P(Actor);
		return d->WorldTransform;
	}

	const math::Matrix4x4& Actor::GetPrevWorldTransform() const
	{
		C_P(Actor);
		return d->PrevWorldTransform;
	}

	Actor::AState Actor::GetState() const
	{
		C_P(Actor);
		return d->State;
	}

	void Actor::SetState(AState State)
	{
		C_P(Actor);
		d->State = State;
	}

	std::shared_ptr<World> Actor::GetWorld() const
	{
		C_P(Actor);
		return d->WorldRef.lock();
	}

	Vector3 Actor::GetForward() const
	{
		C_P(Actor);
		return Vector3::Transform(Vector3::UnitZ, d->Rotation);
	}

	Vector3 Actor::GetRight() const
	{
		C_P(Actor);
		return Vector3::Transform(Vector3::UnitX, d->Rotation);
	}

	Vector3 Actor::GetUp() const
	{
		C_P(Actor);
		return Vector3::Transform(Vector3::UnitY, d->Rotation);
	}

	void Actor::RotateToNewForward(const math::Vector3& Forward)
	{
		// Figure out difference between original (unit z) and new
		float dot = Vector3::Dot(Vector3::UnitZ, Forward);
		float angle = std::acosf(dot);
		// Facing down Z
		if (dot > 0.9999f)
		{
			SetRotation(Quaternion::Identity);
		}
		// Facing down -Z
		else if (dot < -0.9999f)
		{
			SetRotation(Quaternion(Vector3::UnitZ, MATH_PI));
		}
		else
		{
			// Rotate about axis from cross product
			Vector3 axis = Vector3::Cross(Vector3::UnitZ, Forward);
			axis.Normalize();
			SetRotation(Quaternion(axis, angle));
		}
	}

	bool Actor::IsActorPrivateAllocated() const noexcept
	{
		return d_ptr != nullptr;
	}

	void Actor::AddComponent(std::shared_ptr<Component> component)
	{
		C_P(Actor);
		d->Components.push_back(component);
		if (const auto sm = ComponentCast<SceneMeshComponent>(component))
		{
			if (const auto W = GetWorld())
			{
				if (const std::shared_ptr<FScene> scene = W->GetScene())
					scene->AddScenePrimitive(sm);
			}
		}
		if (auto W = GetWorld())
			W->RefreshShadowProjectorForActor(shared_from_this());
	}

	void Actor::RemoveComponent(std::shared_ptr<Component> component)
	{
		C_P(Actor);
		auto iter = std::find(d->Components.begin(), d->Components.end(), component);
		if (iter != d->Components.end())
		{
			if (const auto sm = ComponentCast<SceneMeshComponent>(component))
			{
				if (const auto W = GetWorld())
				{
					if (const std::shared_ptr<FScene> scene = W->GetScene())
						scene->RemoveScenePrimitive(sm);
				}
			}
			d->Components.erase(iter);
			if (auto W = GetWorld())
				W->RefreshShadowProjectorForActor(shared_from_this());
		}
	}

	std::vector<std::shared_ptr<Engine::Component>>& Actor::GetAllComponents() const
	{
		if (!d_ptr)
		{
			// Avoid thread_local std::vector TLS destructor ordering issues (process exit AV in __dyn_tls_dtor):
			// heap vector is intentionally not destroyed per thread.
			thread_local std::vector<std::shared_ptr<Engine::Component>>* s_emptyTls = nullptr;
			if (!s_emptyTls)
				s_emptyTls = new std::vector<std::shared_ptr<Engine::Component>>();
			s_emptyTls->clear();
			return *s_emptyTls;
		}
		C_P(Actor);
		return d->Components;
	}

	void Actor::SetVisible(bool visible)
	{
		C_P(Actor);
		d->visible = visible;
	}

	bool Actor::IsVisible() const
	{
		C_P(Actor);
		return d->visible;
	}

	void Actor::SetActorName(const std::wstring& name)
	{
		C_P(Actor);
		d->ActorName = name;
	}

	std::wstring Actor::GetActorName() const
	{
		C_P(Actor);
		return d->ActorName;
	}

	uint64_t Actor::GetStableInstanceId() const noexcept
	{
		C_P(const Actor);
		return d->StableInstanceId;
	}

	void Actor::ProcessInput(const InputDeviceState& State)
	{
		C_P(Actor);
		for (auto comp : d->Components)
		{
			comp->ProcessInput(State);
		}
	}

}