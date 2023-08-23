#include "Scene/GltfInputComponent.h"
#include "Scene/Actor.h"
#include "win/cpu_clock.h"

namespace Engine
{
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
					Impl->Rotate = Impl->LBtnRotate + (Pos - Impl->LBDownPoint) *  State.DeltaTime /*(1.f / 10.f)*/;
				}
				else if (Impl->RightButtonPressed)
				{
					Impl->Translate = Impl->RBtnDownTranslate + (Pos - Impl->RBDownPoint);
				}

				float xAngle = math::Fmod(Impl->Rotate.x, 360.0f) * math::MATH_PI / 180.f;
				float yAngle = math::Fmod(Impl->Rotate.y, 360.0f) * math::MATH_PI / 180.f;
				math::Quaternion Quat = math::Quaternion::MakeFromEuler(yAngle, xAngle, 0.f);

				GetOwner()->SetRotation(Quat);
			}
			break;
			}
		}
		

	}

}