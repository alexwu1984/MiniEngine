#pragma once
#include <memory>
#include <string>

namespace Engine
{
	class MainEngine;
	class World;
	class GameViewportClient;
	class FWorldSceneRender;

	/** Owns the game World and performs full scene replacement (sync + weak_ptr retarget + LoadScene). */
	class SceneManager
	{
	public:
		SceneManager();

		std::shared_ptr<World> GetWorld() const { return World_; }

		/** Called once engine wiring exists; OwnerEngine used as sig subscriber for unbind on shutdown/reload. */
		void AttachClients(MainEngine* OwnerEngine,
						   const std::shared_ptr<GameViewportClient>& ViewportClient,
						   const std::shared_ptr<FWorldSceneRender>& SceneRender);

		void BindInvalidateToCurrentWorld();
		void UnbindInvalidateFromCurrentWorld();

		void ReloadSceneJson(const std::wstring& JsonPath);

	private:
		std::shared_ptr<World> World_;
		MainEngine* OwnerEngine_ = nullptr;
		std::weak_ptr<GameViewportClient> ViewportClient_;
		std::weak_ptr<FWorldSceneRender> SceneRender_;
	};
}
