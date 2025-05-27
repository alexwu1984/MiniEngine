#include "Scene/GltfInputComponent.h"
#include "Scene/Actor.h"
#include "Scene/SceneView.h"
#include "Scene/CameraComponent.h"
#include "win/cpu_clock.h"

namespace Engine
{
	IMP_COMPONENT_CLASS_NAME(GltfDeviceInputComponent)
	IMP_COMPONENT_TRAITS_CLASS_NAME(GltfDeviceInputComponent)

	struct GltfDeviceInputComponentP
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
		:Component(Owner)
		, Impl(std::make_shared<GltfDeviceInputComponentP>())
	{

	}

	GltfDeviceInputComponent::~GltfDeviceInputComponent()
	{

	}

	void GltfDeviceInputComponent::ProcessInput(const InputDeviceState& State)
	{
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
					Impl->LBDownPoint = Pos;
					Impl->LBtnRotate = Impl->Rotate;
					Impl->LeftButtonPressed = true;

				}
				else if (Button == MouseButton::RightButton)
				{
					Impl->RBDownPoint = Pos;
					Impl->RBtnDownTranslate = Impl->Translate;
					Impl->RightButtonPressed = true;
				}
			}
			break;
			case MET_ButtonUp:
			{
				if (Button == MouseButton::LeftButton)
				{
					Impl->LeftButtonPressed = false;
				}
				else if (Button == MouseButton::RightButton)
				{
					Impl->RightButtonPressed = false;
				}
			}
			break;
			case MET_Move:
			{

				if (Impl->LeftButtonPressed)
				{
					Impl->Rotate = Impl->LBtnRotate + (Pos - Impl->LBDownPoint) * State.DeltaTime;
				}
				else if (Impl->RightButtonPressed)
				{
					Impl->Translate = Impl->RBtnDownTranslate + (Pos - Impl->RBDownPoint) * State.DeltaTime / 20.f;
				}

				float xAngle = -1 * math::Fmod(Impl->Rotate.x, 360.0f) * math::MATH_PI / 180.f;
				float yAngle = math::Fmod(Impl->Rotate.y, 360.0f) * math::MATH_PI / 180.f;
				math::Quaternion Quat = math::Quaternion::MakeFromEuler(yAngle, xAngle, 0.f);

				GetOwner()->SetRotation(Quat);
				//auto Pos = GetOwner()->GetPosition();
				//Pos.x = -1 * Impl->Translate.x;
				//Pos.y = -1 * Impl->Translate.y;
				//GetOwner()->SetPosition(Pos);
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
				MainCamera->SetCameraPos(Mat.TransformPosition(MainCamera->GetCameraPos()));

			}
			break;
			}
		}
		

	}

}