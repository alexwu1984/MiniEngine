#pragma once
#include "core/inc.h"
#include "Scene/DeviceInputState.h"

namespace RenderCore
{
	class RHICommandContext;
}


namespace Engine
{
	class Actor;
	struct ComponentPrivate;
	class CameraComponent;

	class Component : public std::enable_shared_from_this<Component>
	{
	public:
		// Constructor
		// (the lower the update order, the earlier the component updates)
		Component(std::weak_ptr<Actor> Owner);
		// Destructor
		virtual ~Component();

		virtual void InitResource() {};
		// Update this component by delta time
		virtual void Tick(float deltaTime) {};
		virtual void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<CameraComponent> Camera) {}
		// Process input for this component
		virtual void ProcessInput(const InputDeviceState& State) { (State); }
		// Called when world transform changes
		virtual void OnUpdateWorldTransform(float deltaTime) { }

		std::shared_ptr<Actor> GetOwner() const;
	private:
		std::shared_ptr< ComponentPrivate> ImplComponentP;
	};
}