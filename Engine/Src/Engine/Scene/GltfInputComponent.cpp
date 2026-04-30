#include "core/inc.h"
#include "Scene/GltfInputComponent.h"
#include "Scene/Actor.h"
#include "Scene/CameraComponent.h"
#include "win/cpu_clock.h"

namespace Engine
{
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
	};


	GltfDeviceInputComponent::GltfDeviceInputComponent(class std::weak_ptr<Actor> Owner)
		: Component(Owner)
		, d_ptr(new GltfDeviceInputComponentPrivate())
	{

	}

	GltfDeviceInputComponent::~GltfDeviceInputComponent()
	{
		delete d_ptr;
		d_ptr = nullptr;
	}

	void GltfDeviceInputComponent::ProcessInput(const InputDeviceState& State)
	{
		C_P(GltfDeviceInputComponent);
		if (State.Device == DeviceType::NoDevice)
		{
			return;
		}

		if (State.Device == DeviceType::Mouse)
		{
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

				}
				else if (Button == MouseButton::RightButton)
				{
					d->RBDownPoint = Pos;
					d->RBtnDownTranslate = d->Translate;
					d->RightButtonPressed = true;
				}
			}
			break;
			case MET_ButtonUp:
			{
				if (Button == MouseButton::LeftButton)
				{
					d->LeftButtonPressed = false;
				}
				else if (Button == MouseButton::RightButton)
				{
					d->RightButtonPressed = false;
				}
			}
			break;
			case MET_Move:
			{
				static const float RotationSensitivity = 0.15f;
				static const float TranslationSensitivity = 0.01f;

				if (Button == MouseButton::NoButton)
				{
					d->LeftButtonPressed = false;
					d->RightButtonPressed = false;
					break;
				}

				if (d->LeftButtonPressed && Button == MouseButton::LeftButton)
				{
					d->Rotate = d->LBtnRotate + (Pos - d->LBDownPoint) * RotationSensitivity;
				}
				else if (d->RightButtonPressed && Button == MouseButton::RightButton)
				{
					d->Translate = d->RBtnDownTranslate + (Pos - d->RBDownPoint) * TranslationSensitivity;
				}

				float xAngle = -1 * math::Fmod(d->Rotate.x, 360.0f) * math::MATH_PI / 180.f;
				float yAngle = math::Fmod(d->Rotate.y, 360.0f) * math::MATH_PI / 180.f;
				math::Quaternion Quat = math::Quaternion::MakeFromEuler(yAngle, xAngle, 0.f);

				GetOwner()->SetRotation(Quat);
			}
			break;
			case MET_Wheel:
			{
				std::shared_ptr<CameraComponent> MainCamera = GetOwner()->GetComponent<CameraComponent>();
				float Scale = 1.0f;
				if (State.MouseInputState.WheelValue < 0)
				{
					Scale = -0.2f;
				}
				else
				{
					Scale = 0.2f;
				}

				math::Matrix4x4 Mat = math::Matrix4x4::CreateFromTranslate(0, 0, Scale * 1.5f);
				math::Vector3 Target = math::Vector3::UnitZ;
				math::Vector3 NewPos = Mat.TransformPosition(MainCamera->GetCameraPos());
				if (NewPos.GetLength() - Target.GetLength() < 0.01)
				{
					return;
				}
				MainCamera->SetCameraPos(NewPos);

			}
			break;
			}
		}
		

	}

}