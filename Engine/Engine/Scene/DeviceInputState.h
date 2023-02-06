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

	struct MouseInput
	{
		MouseButton Button = NoButton;
		core::vec2f Pos;
	};
}