
#include "Scene/SceneView.h"
#include "Scene/Actor.h"
#include "Scene/CameraComponent.h"
#include "Engine.h"
#include "App/AppWindow.h"

namespace Engine 
{
	struct SceneViewP
	{
		std::vector<std::shared_ptr<Actor>> Actors;
		std::vector<std::shared_ptr<Actor>> PendingActors;
		std::shared_ptr<CameraComponent> MainCamera;

		bool UpdatingActors = false;
		std::mutex DeviceLock;
		std::queue< InputDeviceState> InputStates;
	};

	SceneView::SceneView()
		:Impl(std::make_unique<SceneViewP>())
	{

	}

	SceneView::~SceneView()
	{

	}

	void SceneView::Init()
	{
		auto AppWindow = GEngine->GetAppWindow();
		AppWindow->EvtMouseButtonDown.bind(std::bind(&SceneView::OnMouseButtonDown,this,std::placeholders::_1,std::placeholders::_2), this);
		AppWindow->EvtMouseButtonUp.bind(std::bind(&SceneView::OnMouseButtonUp, this, std::placeholders::_1, std::placeholders::_2), this);
		AppWindow->EvtMouseMove.bind(std::bind(&SceneView::OnMouseMove, this, std::placeholders::_1, std::placeholders::_2), this);
	}

	void SceneView::AddActor(std::shared_ptr<Actor> actor)
	{
		if (Impl->UpdatingActors)
		{
			Impl->PendingActors.emplace_back(actor);
		}
		else
		{
			Impl->Actors.emplace_back(actor);
		}
	}

	void SceneView::RemoveActor(std::shared_ptr<Actor> actor)
	{
		auto iter = std::find(Impl->PendingActors.begin(), Impl->PendingActors.end(), actor);
		if (iter != Impl->PendingActors.end())
		{
			// Swap to end of vector and pop off (avoid erase copies)
			std::iter_swap(iter, Impl->PendingActors.end() - 1);
			Impl->PendingActors.pop_back();
		}

		// Is it in actors?
		iter = std::find(Impl->Actors.begin(), Impl->Actors.end(), actor);
		if (iter != Impl->Actors.end())
		{
			// Swap to end of vector and pop off (avoid erase copies)
			std::iter_swap(iter, Impl->Actors.end() - 1);
			Impl->Actors.pop_back();
		}
	}

	void SceneView::Tick(float DeltaTime)
	{
		InputDeviceState InputState;

		std::queue< InputDeviceState> TmpInputState;
		{
			
			std::lock_guard L(Impl->DeviceLock);
			TmpInputState.swap(Impl->InputStates);
		}

		while (!TmpInputState.empty())
		{
			InputState = TmpInputState.front();
			InputState.DeltaTime = DeltaTime;

			for (auto Item : Impl->Actors)
			{
				Item->ProcessInput(InputState);
			}

			TmpInputState.pop();
		}

		// Update all actors
		Impl->UpdatingActors = true;
		for (auto Item : Impl->Actors)
		{
			Item->Tick(DeltaTime);
		}

		Impl->UpdatingActors = false;

		// Move any pending actors to mActors
		for (auto pending : Impl->PendingActors)
		{
			pending->Tick(DeltaTime);
			Impl->Actors.emplace_back(pending);
		}

		Impl->PendingActors.clear();

		for (auto ItActor = Impl->Actors.begin(); ItActor != Impl->Actors.end();)
		{
			if ((*ItActor)->GetState() == Actor::EDead)
			{
				ItActor = Impl->Actors.erase(ItActor);
			}
			else
			{
				++ItActor;
			}
		}
	}

	void SceneView::SetMainCamera(std::shared_ptr<CameraComponent> Camera)
	{
		Impl->MainCamera = Camera;
	}

	std::shared_ptr<Engine::CameraComponent> SceneView::GetMainCamera() const
	{
		return Impl->MainCamera;
	}

	std::vector<std::shared_ptr<Actor>>& SceneView::GetAllActors() const
	{
		return Impl->Actors;
	}

	void SceneView::OnMouseButtonDown(MouseButton Button, core::vec2f Pos)
	{
		std::lock_guard L(Impl->DeviceLock);
		HandleMouseEvent(MET_ButtonDown, Button, Pos);
	}

	void SceneView::OnMouseButtonUp(MouseButton Button, core::vec2f Pos)
	{
		std::lock_guard L(Impl->DeviceLock);
		HandleMouseEvent(MET_ButtonUp, Button, Pos);
	}

	void SceneView::OnMouseMove(MouseButton Button, core::vec2f Pos)
	{
		std::lock_guard L(Impl->DeviceLock);
		HandleMouseEvent(MET_Move, Button,Pos);
	}

	void SceneView::HandleMouseEvent(MouseEventType EventType,MouseButton Button, core::vec2f Pos)
	{
		InputDeviceState InputState{};
		InputState.Device = Mouse;
		InputState.MouseInputState.EventType = EventType;
		InputState.MouseInputState.Button = Button;
		InputState.MouseInputState.Pos = Pos;
		Impl->InputStates.push(InputState);
	}

}