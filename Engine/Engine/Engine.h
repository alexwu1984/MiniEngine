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
	class AppWindow;
	class SceneView;

	class MainEngine
	{
	public:
		MainEngine();
		~MainEngine();

		void Init(std::shared_ptr< AppWindow> AppWin);
		void ShutDown();

		std::shared_ptr<RenderCore::DynamicRHI> GetRHI() const;
		std::shared_ptr<AppWindow> GetAppWindow()const;
		std::shared_ptr<SceneView> GetScene() const;

	private:
		void Tick();

	private:
		std::shared_ptr<MainEngineP> Impl;
	};

	extern MainEngine* GEngine ;
}