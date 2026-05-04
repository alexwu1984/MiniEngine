#pragma once
#include "Scene/DeviceInputState.h"

namespace Engine
{
	class AppWindow;
	class World;
	struct GameViewportClientPrivate;

	/** Binds window input and drives actor input + world simulation tick (UE viewport client analogue). */
	class GameViewportClient : public std::enable_shared_from_this<GameViewportClient>
	{
	public:
		explicit GameViewportClient(std::weak_ptr<World> InWorld);
		~GameViewportClient();

		void Init(std::shared_ptr<AppWindow> AppWindow);
		/** Bind viewport input/tick context to InWorld (non-owning weak_ptr). UE parallel: viewport SetWorld(InWorld). */
		void SetWorld(std::weak_ptr<World> InWorld);
		/** Drop queued mouse/keyboard-edge events when replacing the world (avoids BS roam input applying to Model3). */
		void ClearPendingInput();

		void Tick(float DeltaTime);

	private:
		void OnMouseButtonDown(MouseButton Button, core::vec2f Pos);
		void OnMouseButtonUp(MouseButton Button, core::vec2f Pos);
		void OnMouseMove(MouseButton Button, core::vec2f Pos);
		void HandleMouseEvent(MouseEventType EventType, MouseButton Button, core::vec2f Pos);
		void OnMouseWheel(int32_t WheelValue);

		GameViewportClientPrivate* d_ptr = nullptr;
	};
}
