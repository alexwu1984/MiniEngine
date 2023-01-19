#pragma once
#include "core/inc.h"

namespace Engine
{
	class Actor;
	struct ComponentP;

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
		virtual void Update(float deltaTime) {};
		//virtual void Draw(FCommandContext& GfxContext, std::shared_ptr<CameraComponent> Camera) {}
		// Process input for this component
		//virtual void ProcessInput(const InputState& State) { (State); }
		// Called when world transform changes
		virtual void OnUpdateWorldTransform() { }

		std::shared_ptr<Actor> GetOwner() const;
	protected:
		std::shared_ptr< ComponentP> ImplComponentP;
	};
}