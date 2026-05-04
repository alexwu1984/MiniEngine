#pragma once
#include "core/inc.h"

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

		/** Called once after engine wiring (weak_ptrs to viewport + scene render). */
		void AttachClients(MainEngine* OwnerEngine,
						   const std::shared_ptr<GameViewportClient>& ViewportClient,
						   const std::shared_ptr<FWorldSceneRender>& SceneRender);

		void ReloadSceneJson(const std::wstring& JsonPath);

	private:
		std::shared_ptr<World> World_;
		MainEngine* OwnerEngine_ = nullptr;
		std::weak_ptr<GameViewportClient> ViewportClient_;
		std::weak_ptr<FWorldSceneRender> SceneRender_;
	};
}
