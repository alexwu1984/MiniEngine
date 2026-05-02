#pragma once
#include "core/inc.h"
#include "Scene/DeviceInputState.h"
#include "ActorTraits.h"

namespace RenderCore
{
	class RHICommandContext;
}


namespace Engine
{
	class Actor;
	struct ComponentPrivate;
	class CameraComponent;

#define DECLARE_COMPONENT_CLASS_NAME(ClassName)\
	public:\
		static std::string Name;\
		virtual std::string GetName() {return Name;}

#define IMP_COMPONENT_CLASS_NAME(ClassName)\
	std::string ClassName::Name = {#ClassName};

	class Component : public std::enable_shared_from_this<Component>
	{
	public:
		enum Flag : uint8_t
		{
			IsComponent = true,
		};
		// Constructor
		// (the lower the update order, the earlier the component updates)
		DECLARE_COMPONENT_CLASS_NAME(Component)
		Component(std::weak_ptr<Actor> Owner);
		// Destructor
		virtual ~Component();

		virtual void InitResource() {};
		// Update this component by delta time
		virtual void Tick(float deltaTime) {};
		// Process input for this component
		virtual void ProcessInput(const InputDeviceState& State) { (State); }
		// Called when world transform changes
		virtual void OnUpdateWorldTransform(float deltaTime) { }

		std::shared_ptr<Actor> GetOwner() const;
		uint64_t GetStableComponentInstanceId() const noexcept;

	private:
		std::shared_ptr< ComponentPrivate> ImplComponentP;
	};

	DECLARE_COMPONENT_TRAITS_CLASS_NAME(Component)

	template<typename TComponentType>
	static __forceinline std::shared_ptr<TComponentType> ComponentCast(std::shared_ptr<Component> Resource)
	{
		static_assert(TComponentType::Flag::IsComponent);

		if (Resource->GetName() == ComponentTraitsClassName<TComponentType>::Name)
		{
			return std::static_pointer_cast<TComponentType>(Resource);
		}
		return nullptr;
	}

	template<typename TComponent>
	static __forceinline std::shared_ptr<TComponent>  GetComponent(const std::vector<std::shared_ptr<Component>>& Components)
	{
		if (Components.empty())
		{
			return {};
		}

		for (const auto& Comp : Components)
		{
			auto Temp = ComponentCast<TComponent>(Comp);
			if (Temp)
			{
				return Temp;
			}
		}
		return {};
	}
}