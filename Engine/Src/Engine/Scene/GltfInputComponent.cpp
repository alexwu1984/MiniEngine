#include "core/inc.h"
#include "Scene/GltfInputComponent.h"
#include "Scene/Actor.h"
#include "Scene/CameraComponent.h"
#include "Scene/FreeRoamCameraComponent.h"
#include "Scene/DeviceInputState.h"
#include "win/cpu_clock.h"
#include "win/win32.h"
#include <algorithm>
#include <cmath>

namespace Engine
{
	namespace
	{
		static math::Vector3 PolarToVectorGlTFSample(float yaw, float pitch)
		{
			return math::Vector3(sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch));
		}
	} // namespace
	IMP_COMPONENT_CLASS_NAME(GltfDeviceInputComponent)
	IMP_COMPONENT_TRAITS_CLASS_NAME(GltfDeviceInputComponent)

	struct GltfDeviceInputComponentPrivate
	{
		core::vec2f LBDownPoint;
		core::vec2f RBDownPoint;
		core::vec2f Rotate;
		core::vec2f Translate;
		core::vec2f LBtnDownTranslate;
		core::vec2f LBtnRotate;
		core::vec2f RBtnDownTranslate;
		bool LeftButtonPressed = false;
		bool RightButtonPressed = false;
		bool bMouseRotateModelEnabled = true;
		core::vec2f RoamLookLastPos{};
		bool bRoamLookHasLast = false;

		bool bOrbitCameraEnabled = false;
		math::Vector3 OrbitTargetWorld{};
		float OrbitYaw = 0.f;
		float OrbitPitch = 0.f;
		float OrbitDistance = 3.5f;
		core::vec2f OrbitDragLastPos{};
	};

	GltfDeviceInputComponent::GltfDeviceInputComponent(class std::weak_ptr<Actor> Owner)
		: Component(std::move(Owner))
		, d_ptr(new GltfDeviceInputComponentPrivate())
	{
	}

	GltfDeviceInputComponent::~GltfDeviceInputComponent()
	{
		delete d_ptr;
		d_ptr = nullptr;
	}

	void GltfDeviceInputComponent::SetMouseRotateModelEnabled(bool bEnabled)
	{
		C_P(GltfDeviceInputComponent);
		d->bMouseRotateModelEnabled = bEnabled;
	}

	bool GltfDeviceInputComponent::GetMouseRotateModelEnabled() const
	{
		C_P(const GltfDeviceInputComponent);
		return d->bMouseRotateModelEnabled;
	}

	void GltfDeviceInputComponent::EnableOrbitCamera(bool bEnable, const math::Vector3& targetWorld, float distance, float yawRadians,
													 float pitchRadians)
	{
		C_P(GltfDeviceInputComponent);
		d->bOrbitCameraEnabled = bEnable;
		if (!bEnable)
			return;
		d->OrbitTargetWorld = targetWorld;
		d->OrbitDistance = std::max(distance, 0.1f);
		d->OrbitYaw = yawRadians;
		d->OrbitPitch = pitchRadians;
		const float lim = math::MATH_PI * 0.5f - 1e-3f;
		d->OrbitPitch = std::clamp(d->OrbitPitch, -lim, lim);
	}

	bool GltfDeviceInputComponent::IsOrbitCameraEnabled() const
	{
		C_P(const GltfDeviceInputComponent);
		return d->bOrbitCameraEnabled;
	}

	void GltfDeviceInputComponent::SnapOrbitToCamera(CameraComponent* Cam)
	{
		C_P(GltfDeviceInputComponent);
		if (!Cam || !d->bOrbitCameraEnabled)
			return;
		const math::Vector3 pol = PolarToVectorGlTFSample(d->OrbitYaw, d->OrbitPitch);
		const math::Vector3 eye = d->OrbitTargetWorld + pol * d->OrbitDistance;
		Cam->SetExplicitLookAtWorldTarget(d->OrbitTargetWorld, true);
		Cam->SetCameraPos(eye);
	}

	void GltfDeviceInputComponent::ProcessInput(const InputDeviceState& State)
	{
		C_P(GltfDeviceInputComponent);
		if (State.Device == DeviceType::NoDevice)
			return;

		if (State.Device == DeviceType::KeyboardFrame)
		{
			if (const auto roam = GetOwner()->GetComponent<FreeRoamCameraComponent>())
				roam->ApplyKeyboardNavigation(State.Keyboard, State.DeltaTime);
			return;
		}

		if (State.Device != DeviceType::Mouse)
			return;

		const auto roamCam = GetOwner()->GetComponent<FreeRoamCameraComponent>();
		const auto& Pos = State.MouseInputState.Pos;
		const auto& Button = State.MouseInputState.Button;

		switch (State.MouseInputState.EventType)
		{
		case MET_ButtonDown:
		{
			if (Button == MouseButton::LeftButton)
			{
				d->LBDownPoint = Pos;
				d->LBtnRotate = d->Rotate;
				d->LeftButtonPressed = true;
				if (d->bOrbitCameraEnabled)
					d->OrbitDragLastPos = Pos;
			}
			else if (Button == MouseButton::RightButton)
			{
				d->RBDownPoint = Pos;
				d->RBtnDownTranslate = d->Translate;
				d->RightButtonPressed = true;
				if (roamCam)
				{
					d->RoamLookLastPos = Pos;
					d->bRoamLookHasLast = true;
				}
			}
		}
		break;
		case MET_ButtonUp:
		{
			if (Button == MouseButton::LeftButton)
				d->LeftButtonPressed = false;
			else if (Button == MouseButton::RightButton)
			{
				d->RightButtonPressed = false;
				d->bRoamLookHasLast = false;
			}
		}
		break;
		case MET_Move:
		{
			static const float RotationSensitivity = 0.15f;
			static const float TranslationSensitivity = 0.01f;

			// Roam look: apply before NoButton handling. WM_MOUSEMOVE often has wParam==0 even while RMB is held;
			// the old path cleared RightButtonPressed on NoButton and required Button==RightButton, so look never worked reliably.
			if (roamCam && d->RightButtonPressed && d->bRoamLookHasLast)
			{
				const core::vec2f delta{ Pos.x - d->RoamLookLastPos.x, Pos.y - d->RoamLookLastPos.y };
				d->RoamLookLastPos = Pos;
				roamCam->ApplyMouseLookPixelDelta(delta.x, delta.y);
			}

			if (Button == MouseButton::NoButton)
			{
				d->LeftButtonPressed = false;
				if (!roamCam || !d->RightButtonPressed)
				{
					d->RightButtonPressed = false;
					d->bRoamLookHasLast = false;
				}
				break;
			}

			if (roamCam)
				break;

			if (d->bOrbitCameraEnabled)
			{
				std::shared_ptr<CameraComponent> orbitCam = GetOwner()->GetComponent<CameraComponent>();
				if (orbitCam && d->LeftButtonPressed && Button == MouseButton::LeftButton)
				{
					const core::vec2f delta{ Pos.x - d->OrbitDragLastPos.x, Pos.y - d->OrbitDragLastPos.y };
					d->OrbitDragLastPos = Pos;
					const bool ctrlDown = (::GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
					if (ctrlDown)
					{
						const math::Vector3 pol = PolarToVectorGlTFSample(d->OrbitYaw, d->OrbitPitch);
						const math::Vector3 eye = d->OrbitTargetWorld + pol * d->OrbitDistance;
						math::Vector3 forward = d->OrbitTargetWorld - eye;
						forward.Normalize();
						math::Vector3 worldUp(0.f, 1.f, 0.f);
						math::Vector3 right = math::Vector3::Cross(worldUp, forward);
						if (right.GetSqrLength() < 1e-10f)
							right = math::Vector3::Cross(math::Vector3(1.f, 0.f, 0.f), forward);
						right.Normalize();
						const math::Vector3 camUp = math::Vector3::Cross(forward, right).Normalize();
						const float panScale = d->OrbitDistance / 10.f;
						d->OrbitTargetWorld += right * ((-delta.x) / 100.f) * panScale;
						d->OrbitTargetWorld += camUp * ((delta.y) / 100.f) * panScale;
					}
					else
					{
						d->OrbitYaw -= delta.x / 100.f;
						d->OrbitPitch += delta.y / 100.f;
						const float lim = math::MATH_PI * 0.5f - 1e-3f;
						d->OrbitPitch = std::clamp(d->OrbitPitch, -lim, lim);
					}
					SnapOrbitToCamera(orbitCam.get());
					break;
				}
			}

			if (!d->bMouseRotateModelEnabled)
				break;

			if (d->LeftButtonPressed && Button == MouseButton::LeftButton)
				d->Rotate = d->LBtnRotate + (Pos - d->LBDownPoint) * RotationSensitivity;
			else if (d->RightButtonPressed && Button == MouseButton::RightButton)
				d->Translate = d->RBtnDownTranslate + (Pos - d->RBDownPoint) * TranslationSensitivity;

			const float xAngle = -1.f * math::Fmod(d->Rotate.x, 360.0f) * math::MATH_PI / 180.f;
			const float yAngle = math::Fmod(d->Rotate.y, 360.0f) * math::MATH_PI / 180.f;
			const math::Quaternion quat = math::Quaternion::MakeFromEuler(yAngle, xAngle, 0.f);
			GetOwner()->SetRotation(quat);
		}
		break;
		case MET_Wheel:
		{
			if (roamCam)
			{
				roamCam->ApplyWheelZoom(State.MouseInputState.WheelValue);
				break;
			}
			std::shared_ptr<CameraComponent> MainCamera = GetOwner()->GetComponent<CameraComponent>();
			if (!MainCamera)
				break;
			if (d->bOrbitCameraEnabled)
			{
				d->OrbitDistance -= static_cast<float>(State.MouseInputState.WheelValue) / 360.f;
				d->OrbitDistance = std::max(d->OrbitDistance, 0.1f);
				SnapOrbitToCamera(MainCamera.get());
				break;
			}
			float Scale = 1.0f;
			if (State.MouseInputState.WheelValue < 0)
				Scale = -0.2f;
			else
				Scale = 0.2f;

			const math::Matrix4x4 Mat = math::Matrix4x4::CreateFromTranslate(0, 0, Scale * 1.5f);
			const math::Vector3 Target = math::Vector3::UnitZ;
			const math::Vector3 NewPos = Mat.TransformPosition(MainCamera->GetCameraPos());
			if (NewPos.GetLength() - Target.GetLength() < 0.01f)
				return;
			MainCamera->SetCameraPos(NewPos);
		}
		break;
		default:
			break;
		}
	}

} // namespace Engine
