#include "Engine/Scene/Component.h"

namespace Engine
{
	struct ComponentPrivate
	{
		std::weak_ptr<Actor> Owner;
	};

	Component::Component(std::weak_ptr<Actor> Owner)
		:ImplComponentP(std::make_shared<ComponentPrivate>())
	{
		ImplComponentP->Owner = Owner;
	}

	Component::~Component()
	{

	}

	std::shared_ptr<Actor> Component::GetOwner() const
	{
		return ImplComponentP->Owner.lock();
	}

}