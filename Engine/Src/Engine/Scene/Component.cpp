#include "Engine/Scene/Component.h"
#include "Render/RenderStableIds.h"

namespace Engine
{
	IMP_COMPONENT_CLASS_NAME(Component)
	IMP_COMPONENT_TRAITS_CLASS_NAME(Component)

	struct ComponentPrivate
	{
		uint64_t StableInstanceId = 0;
		std::weak_ptr<Actor> Owner;
	};

	Component::Component(std::weak_ptr<Actor> Owner)
		:ImplComponentP(std::make_shared<ComponentPrivate>())
	{
		ImplComponentP->Owner = Owner;
		ImplComponentP->StableInstanceId = AllocateComponentStableInstanceId();
	}

	Component::~Component()
	{

	}

	std::shared_ptr<Actor> Component::GetOwner() const
	{
		return ImplComponentP->Owner.lock();
	}

	uint64_t Component::GetStableComponentInstanceId() const noexcept
	{
		return ImplComponentP->StableInstanceId;
	}

}