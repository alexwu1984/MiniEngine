#pragma once
#include "RHI/DynamicRHI.h"
#include "tinygltf/json.h"

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
	struct MainEnginePrivate;
	class AppWindow;
	class World;
	class GameViewportClient;
	class SceneRender;

	class MainEngine
	{
	public:
		MainEngine();
		~MainEngine();

		void Init(std::shared_ptr< AppWindow> AppWin,RenderCore::RHIAPIType ApiType);
		void StartThread();
		void ShutDown();
		void LoadConfig(const std::wstring& FileName, const nlohmann::json& Root);
		std::wstring GetModelPath() const;
		std::shared_ptr<RenderCore::DynamicRHI> GetRHI() const;
		std::shared_ptr<AppWindow> GetAppWindow()const;
		std::shared_ptr<World> GetWorld() const;
		std::shared_ptr<GameViewportClient> GetViewportClient() const;
		std::shared_ptr<SceneRender> GetSceneRender() const;

	private:
		void Tick(float DeltaTime);
		void OnSizeChanged(core::vec2i NewSize);
	private:
		MainEnginePrivate* d_ptr = nullptr;
	};

	extern MainEngine* GEngine ;
}