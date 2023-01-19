#include "Engine/Scene/Actor.h"
#include "Engine/Scene/ActorPrivate.h"

namespace Engine
{
	template<typename TActorType>
	std::shared_ptr<TActorType> ActorCast(std::shared_ptr<Actor> Resource)
	{
		static_assert(TActorType::Flag::IsActor);

		if (Resource->GetName() == ActorTraitsClassName<TActorType>::Name)
		{
			return std::static_pointer_cast<TActorType>(Resource);
		}
		return nullptr;
	}

	IMP_ACTOR_CLASS_NAME(Actor)

	Actor::Actor(std::weak_ptr<Scene> world)
		:ImplActorP(std::make_shared<ActorP>())
	{

	}
	Actor::~Actor()
	{

	}

	void Actor::InitResouce()
	{

	}

	void Actor::Update(float deltaTime)
	{

	}

	void Actor::UpdateComponents(float deltaTime)
	{

	}

	void Actor::UpdateActor(float deltaTime)
	{

	}

	math::Vector3 Actor::GetPosition() const
	{
		return ImplActorP->Position;
	}

	void Actor::SetPosition(const math::Vector3& pos)
	{
		ImplActorP->Position = pos;
	}

	float Actor::GetScale() const
	{
		return ImplActorP->Scale;
	}

	void Actor::SetScale(float scale)
	{
		ImplActorP->Scale = scale;
	}

	math::Quaternion Actor::GetRotation() const
	{
		return ImplActorP->Rotation;
	}

	void Actor::SetRotation(const math::Quaternion& rotation)
	{
		ImplActorP->Rotation = rotation;
	}

}