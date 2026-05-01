#pragma once
#include "Scene/Component.h"

namespace Engine
{
	struct GltfDeviceInputComponentPrivate;

	class GltfDeviceInputComponent : public Component
	{
	public:
		DECLARE_COMPONENT_CLASS_NAME(GltfDeviceInputComponent)
		GltfDeviceInputComponent(class std::weak_ptr<Actor> Owner);
		~GltfDeviceInputComponent();

		virtual void ProcessInput(const InputDeviceState& State);

		/** When false, LMB drag no longer rotates the owner (e.g. scene uses RoamCamera). JSON can override per actor. */
		void SetMouseRotateModelEnabled(bool bEnabled);
		bool GetMouseRotateModelEnabled() const;

	private:
		GltfDeviceInputComponentPrivate* d_ptr = nullptr;
	};
	DECLARE_COMPONENT_TRAITS_CLASS_NAME(GltfDeviceInputComponent)
}