#pragma once
#include "core/inc.h"

namespace Engine
{
	class MainEngine
	{
	public:
		MainEngine();
		~MainEngine();
	};

	extern MainEngine* GEngine = nullptr;
}