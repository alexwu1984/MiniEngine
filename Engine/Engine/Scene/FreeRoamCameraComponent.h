#pragma once
#include "Scene/CameraComponent.h"
#include "Scene/DeviceInputState.h"

namespace Engine
{
	/** FPS-style camera; input is driven by GltfDeviceInputComponent (mouse + KeyboardFrame). */
	class FreeRoamCameraComponent final : public CameraComponent
	{
	public:
		DECLARE_COMPONENT_CLASS_NAME(FreeRoamCameraComponent)
		explicit FreeRoamCameraComponent(std::weak_ptr<Actor> Owner);
		~FreeRoamCameraComponent() override = default;

		void InitResource() override;
		void Tick(float DeltaTime) override;

		void SetMoveSpeed(float InSpeed) { MoveSpeed = InSpeed; }
		void SetLookSensitivity(float InSens) { LookSensitivity = InSens; }

		void ApplyMouseLookPixelDelta(float Dx, float Dy);
		void ApplyKeyboardNavigation(const KeyboardFrameInput& Keys, float DeltaTime);
		void ApplyWheelZoom(int32_t WheelNotches);

		/** Set initial yaw/pitch so the camera at EyeWorld looks toward TargetWorld (same convention as Tick). */
		void SetInitialLookToward(const math::Vector3& EyeWorld, const math::Vector3& TargetWorld);
		/** Applied after SetInitialLookToward (e.g. JSON LookYawOffsetDegrees when asset faces -Z). */
		void AddInitialYawPitchOffset(float yawDeg, float pitchDeg);

	private:
		float YawDegrees = 0.f;
		float PitchDegrees = 0.f;
		float MoveSpeed = 5.f;
		float LookSensitivity = 0.12f;
	};
	DECLARE_COMPONENT_TRAITS_CLASS_NAME(FreeRoamCameraComponent);
}
