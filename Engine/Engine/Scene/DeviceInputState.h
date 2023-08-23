#pragma once
#include "core/inc.h"

namespace Engine
{
	enum MouseButton :uint8_t
	{
		NoButton = 0,
		LeftButton = 1,
		RightButton = 2,
	};

	enum MouseEventType : uint8_t
	{
		MET_NoEvent = 0,
		MET_ButtonDown,
		MET_ButtonUp,
		MET_Move,
	};

	struct MouseInput
	{
		MouseEventType EventType = MET_NoEvent;
		MouseButton Button = NoButton;
		core::vec2f Pos;
	};

	enum DeviceType
	{
		NoDevice = 0,
		Mouse,
	};

	struct InputDeviceState
	{
		DeviceType Device = NoDevice;
		MouseInput MouseInputState;
		float DeltaTime{ 0 };
	};
}