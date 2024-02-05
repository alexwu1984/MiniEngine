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
	class SceneRender;

	class MainEngine
	{
	public:
		MainEngine();
		~MainEngine();

		void Init(std::shared_ptr< AppWindow> AppWin);
		void StartThread();
		void ShutDown();
		void LoadConfig(const std::wstring& FileName);
		std::wstring GetModelPath() const;
		std::shared_ptr<RenderCore::DynamicRHI> GetRHI() const;
		std::shared_ptr<AppWindow> GetAppWindow()const;
		std::shared_ptr<SceneView> GetScene() const;
		std::shared_ptr<SceneRender> GetSceneRender() const;

	private:
		void Tick(float DeltaTime);
		void OnSizeChanged(core::vec2i NewSize);
	private:
		std::shared_ptr<MainEngineP> Impl;
	};

	extern MainEngine* GEngine ;
}