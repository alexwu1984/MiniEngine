#pragma once
#include "core/inc.h"

namespace Engine
{
	enum MouseButton :uint8_t
	{
		NoButton = 0,
		LeftButton = 1,
		RightButton = 2,
		MButton = 3,
	};

	enum MouseEventType : uint8_t
	{
		MET_NoEvent = 0,
		MET_ButtonDown,
		MET_ButtonUp,
		MET_Move,
		MET_Wheel,
	};

	struct MouseInput
	{
		MouseEventType EventType = MET_NoEvent;
		MouseButton Button = NoButton;
		core::vec2f Pos;
		int32_t WheelValue{ 0 };
	};

	enum DeviceType
	{
		NoDevice = 0,
		Mouse,
		/** Once per frame after mouse queue; used for WASD / modifier keys (see GameViewportClient). */
		KeyboardFrame,
	};

	/** Snapshot for one tick; filled by GameViewportClient using GetAsyncKeyState. */
	struct KeyboardFrameInput
	{
		bool bW = false;
		bool bA = false;
		bool bS = false;
		bool bD = false;
		bool bSpace = false;
		bool bCtrl = false;
	};

	struct InputDeviceState
	{
		DeviceType Device = NoDevice;
		MouseInput MouseInputState;
		KeyboardFrameInput Keyboard;
		float DeltaTime{ 0 };
	};
}