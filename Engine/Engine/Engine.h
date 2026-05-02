#pragma once
#include "RHI/DynamicRHI.h"
#include "tinygltf/json.h"
#include <functional>

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
	class FWorldSceneRender;
	class SceneManager;

	class MainEngine
	{
	public:
		MainEngine();
		~MainEngine();

		void Init(std::shared_ptr< AppWindow> AppWin,RenderCore::RHIAPIType ApiType);
		void StartThread();
		void ShutDown();
		void LoadConfig(const std::wstring& FileName, const nlohmann::json& Root);
		/** Delegates to SceneManager::ReloadSceneJson (replace World + LoadScene). */
		void ReloadSceneJson(const std::wstring& JsonPath);
		std::shared_ptr<SceneManager> GetSceneManager() const;
		std::wstring GetModelPath() const;
		std::shared_ptr<RenderCore::DynamicRHI> GetRHI() const;
		std::shared_ptr<AppWindow> GetAppWindow()const;
		std::shared_ptr<World> GetWorld() const;
		std::shared_ptr<GameViewportClient> GetViewportClient() const;
		std::shared_ptr<FWorldSceneRender> GetSceneRender() const;
		/** Runs at end of each game-thread Tick after Present (after optional Resize enqueue). Safe place for ReloadSceneJson / heavy scene mutations. */
		void SetEndFrameTickCallback(std::function<void()> Callback);

	private:
		void Tick(float DeltaTime);
		void OnSizeChanged(core::vec2i NewSize);
	private:
		MainEnginePrivate* d_ptr = nullptr;
	};

	extern MainEngine* GEngine ;
}