#pragma once
#include "core/inc.h"

namespace core
{
	class CommandLine;
}

namespace RenderCore
{
	class DynamicRHI;
}

namespace Engine
{
	struct MainEngineP;

	class MainEngine
	{
	public:
		MainEngine();
		~MainEngine();

		void Init();

		std::shared_ptr<RenderCore::DynamicRHI> GetRHI() const;

	private:
		std::shared_ptr<MainEngineP> Data;
	};

	extern MainEngine* GEngine ;
}