#pragma once
#include "Scene/Component.h"

namespace Engine
{
	struct GltfDeviceInputComponentP;

	class GltfDeviceInputComponent : public Component
	{
	public:
		GltfDeviceInputComponent(class std::weak_ptr<Actor> Owner);
		~GltfDeviceInputComponent();

		virtual void ProcessInput(const InputDeviceState& State);

	private:
		std::shared_ptr< GltfDeviceInputComponentP> Impl;
	};
}