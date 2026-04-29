#include "Scene/GameViewportClient.h"
#include "Scene/World.h"
#include "App/AppWindow.h"
#include "Engine.h"

namespace Engine
{
	struct GameViewportClientPrivate
	{
		std::weak_ptr<World> WorldRef;
		std::mutex DeviceLock;
		std::queue<InputDeviceState> InputStates;
	};

	GameViewportClient::GameViewportClient(std::weak_ptr<World> InWorld)
		: d_ptr(new GameViewportClientPrivate{ std::move(InWorld), {}, {} })
	{
	}

	GameViewportClient::~GameViewportClient()
	{
		if (GEngine)
		{
			if (auto win = GEngine->GetAppWindow())
			{
				win->EvtMouseButtonDown.unbind(this);
				win->EvtMouseButtonUp.unbind(this);
				win->EvtMouseMove.unbind(this);
				win->EvtMouseWheel.unbind(this);
			}
		}
		delete d_ptr;
	}

	void GameViewportClient::Init(std::shared_ptr<AppWindow> AppWindow)
	{
		AppWindow->EvtMouseButtonDown.bind(std::bind(&GameViewportClient::OnMouseButtonDown, this, std::placeholders::_1, std::placeholders::_2), this);
		AppWindow->EvtMouseButtonUp.bind(std::bind(&GameViewportClient::OnMouseButtonUp, this, std::placeholders::_1, std::placeholders::_2), this);
		AppWindow->EvtMouseMove.bind(std::bind(&GameViewportClient::OnMouseMove, this, std::placeholders::_1, std::placeholders::_2), this);
		AppWindow->EvtMouseWheel.bind(std::bind(&GameViewportClient::OnMouseWheel, this, std::placeholders::_1), this);
	}

	void GameViewportClient::Tick(float DeltaTime)
	{
		C_P(GameViewportClient);
		auto World = d->WorldRef.lock();
		if (!World)
			return;

		std::queue<InputDeviceState> TmpInputState;
		{
			std::lock_guard Lock(d->DeviceLock);
			TmpInputState.swap(d->InputStates);
		}

		while (!TmpInputState.empty())
		{
			InputDeviceState InputState = TmpInputState.front();
			InputState.DeltaTime = DeltaTime;
			World->DispatchInput(InputState);
			TmpInputState.pop();
		}

		World->TickSimulation(DeltaTime);
	}

	void GameViewportClient::OnMouseButtonDown(MouseButton Button, core::vec2f Pos)
	{
		C_P(GameViewportClient);
		std::lock_guard Lock(d->DeviceLock);
		HandleMouseEvent(MET_ButtonDown, Button, Pos);
	}

	void GameViewportClient::OnMouseButtonUp(MouseButton Button, core::vec2f Pos)
	{
		C_P(GameViewportClient);
		std::lock_guard Lock(d->DeviceLock);
		HandleMouseEvent(MET_ButtonUp, Button, Pos);
	}

	void GameViewportClient::OnMouseMove(MouseButton Button, core::vec2f Pos)
	{
		C_P(GameViewportClient);
		std::lock_guard Lock(d->DeviceLock);
		HandleMouseEvent(MET_Move, Button, Pos);
	}

	void GameViewportClient::HandleMouseEvent(MouseEventType EventType, MouseButton Button, core::vec2f Pos)
	{
		C_P(GameViewportClient);
		InputDeviceState InputState{};
		InputState.Device = Mouse;
		InputState.MouseInputState.EventType = EventType;
		InputState.MouseInputState.Button = Button;
		InputState.MouseInputState.Pos = Pos;
		d->InputStates.emplace(InputState);
	}

	void GameViewportClient::OnMouseWheel(int32_t WheelValue)
	{
		C_P(GameViewportClient);
		std::lock_guard Lock(d->DeviceLock);
		InputDeviceState InputState{};
		InputState.Device = Mouse;
		InputState.MouseInputState.EventType = MouseEventType::MET_Wheel;
		InputState.MouseInputState.WheelValue = WheelValue;
		d->InputStates.emplace(InputState);
	}
}
