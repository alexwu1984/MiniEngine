#pragma once
#include "Scene/Component.h"
#include "math/vector3.h"

namespace Engine
{
	class CameraComponent;
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

		/** AMD glTFSample-style orbit: LMB orbit, Ctrl+LMB pan, wheel zoom (owner must have CameraComponent). */
		void EnableOrbitCamera(bool bEnable, const math::Vector3& targetWorld, float distance, float yawRadians, float pitchRadians);
		bool IsOrbitCameraEnabled() const;
		void SnapOrbitToCamera(CameraComponent* Cam);

	private:
		GltfDeviceInputComponentPrivate* d_ptr = nullptr;
	};
	DECLARE_COMPONENT_TRAITS_CLASS_NAME(GltfDeviceInputComponent)
}