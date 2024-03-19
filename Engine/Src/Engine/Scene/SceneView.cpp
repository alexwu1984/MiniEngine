
#include "Scene/SceneView.h"
#include "Scene/GltfActor.h"
#include "Scene/CameraComponent.h"
#include "Engine.h"
#include "App/AppWindow.h"
#include "Render/MaterialPreFrame.h"

namespace Engine 
{
	struct SceneViewPrivate
	{
		std::vector<std::shared_ptr<Actor>> Actors;
		std::vector<std::shared_ptr<Actor>> PendingActors;
		std::shared_ptr<CameraComponent> MainCamera;

		bool UpdatingActors = false;
		std::mutex DeviceLock;
		std::queue< InputDeviceState> InputStates;
		std::vector< Light> lightInfos;
		std::recursive_mutex lock;
	};

	SceneView::SceneView()
		:d_ptr(new SceneViewPrivate())
	{

	}

	SceneView::~SceneView()
	{
		delete d_ptr;
	}

	void SceneView::Init()
	{
		auto AppWindow = GEngine->GetAppWindow();
		AppWindow->EvtMouseButtonDown.bind(std::bind(&SceneView::OnMouseButtonDown,this,std::placeholders::_1,std::placeholders::_2), this);
		AppWindow->EvtMouseButtonUp.bind(std::bind(&SceneView::OnMouseButtonUp, this, std::placeholders::_1, std::placeholders::_2), this);
		AppWindow->EvtMouseMove.bind(std::bind(&SceneView::OnMouseMove, this, std::placeholders::_1, std::placeholders::_2), this);
		AppWindow->EvtMouseWheel.bind(std::bind(&SceneView::OnMouseWheel, this, std::placeholders::_1),this);
	}

	void SceneView::LoadScene(const std::wstring& ModelFile)
	{
		C_P(SceneView);
		Engine::GEngine->LoadConfig(ModelFile);

		try
		{
			nlohmann::json Root;
			std::ifstream input_json_file(ModelFile);
			if (!input_json_file.is_open())
			{
				return;
			}

			RemoveAllActors();

			input_json_file >> Root;
			nlohmann::json Models = Root["Modles"];
			for (const auto &Model: Models)
			{
				auto AGltfModel = std::make_shared<Engine::GltfActor>(this->shared_from_this(), Model);
				AGltfModel->InitResouce();
				AddActor(AGltfModel);
			}

			nlohmann::json evnJson = Root["Evn"];
			const nlohmann::json lightJsons = evnJson["Light"];
			for (const auto& lightInfoJson : lightJsons)
			{
				Light lightInfo;
				std::string colorStr = lightInfoJson["LightColor"];
				std::sscanf(colorStr.c_str(), "%f,%f,%f", &lightInfo.Color.x, &lightInfo.Color.y, &lightInfo.Color.z);
				std::string dirStr = lightInfoJson["LightDir"];
				std::sscanf(dirStr.c_str(), "%f,%f,%f", &lightInfo.Direction.x, &lightInfo.Direction.y, &lightInfo.Direction.z);
				lightInfo.Type = lightInfoJson["LightType"];
				lightInfo.Intensity = lightInfoJson["LightStrength"];
				d->lightInfos.emplace_back(lightInfo);
			}

		}
		catch (const std::exception& e)
		{
			std::string error = e.what();
		}

	}

	void SceneView::AddActor(std::shared_ptr<Actor> actor)
	{
		C_P(SceneView);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		if (d->UpdatingActors)
		{
			d->PendingActors.emplace_back(actor);
		}
		else
		{
			d->Actors.emplace_back(actor);
		}
	}

	void SceneView::RemoveActor(std::shared_ptr<Actor> actor)
	{
		C_P(SceneView);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		auto iter = std::find(d->PendingActors.begin(), d->PendingActors.end(), actor);
		if (iter != d->PendingActors.end())
		{
			// Swap to end of vector and pop off (avoid erase copies)
			std::iter_swap(iter, d->PendingActors.end() - 1);
			d->PendingActors.pop_back();
		}

		// Is it in actors?
		iter = std::find(d->Actors.begin(), d->Actors.end(), actor);
		if (iter != d->Actors.end())
		{
			// Swap to end of vector and pop off (avoid erase copies)
			std::iter_swap(iter, d->Actors.end() - 1);
			d->Actors.pop_back();
		}
	}

	void SceneView::RemoveAllActors()
	{
		C_P(SceneView);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		for (auto ItActor = d->Actors.begin(); ItActor != d->Actors.end(); ++ItActor)
		{
			(*ItActor)->SetState(Actor::EDead);
		}
	}

	void SceneView::Tick(float DeltaTime)
	{
		C_P(SceneView);
		InputDeviceState InputState;
		std::queue< InputDeviceState> TmpInputState;
		{
			
			std::lock_guard Lock(d->DeviceLock);
			TmpInputState.swap(d->InputStates);
		}
		
		while (!TmpInputState.empty())
		{
			InputState = TmpInputState.front();
			InputState.DeltaTime = DeltaTime;

			std::lock_guard<std::recursive_mutex> l(d->lock);
			for (auto Item : d->Actors)
			{
				if (Item->GetState() == Actor::EActive)
				{
					Item->ProcessInput(InputState);
				}
				
			}

			TmpInputState.pop();
		}

		std::lock_guard<std::recursive_mutex> l(d->lock);
		// Update all actors
		d->UpdatingActors = true;
		for (auto Item : d->Actors)
		{
			if (Item->GetState() == Actor::EActive)
			{
				Item->Tick(DeltaTime);
			}
		}

		d->UpdatingActors = false;

		// Move any pending actors to mActors
		for (auto pending : d->PendingActors)
		{
			pending->Tick(DeltaTime);
			d->Actors.emplace_back(pending);
		}

		d->PendingActors.clear();

		//for (auto ItActor = d->Actors.begin(); ItActor != d->Actors.end();)
		//{
		//	if ((*ItActor) && (*ItActor)->GetState() == Actor::EDead)
		//	{
		//		ItActor = d->Actors.erase(ItActor);
		//	}
		//	else
		//	{
		//		++ItActor;
		//	}
		//}
	}

	void SceneView::SetMainCamera(std::shared_ptr<CameraComponent> Camera)
	{
		C_P(SceneView);
		d->MainCamera = Camera;
	}

	std::shared_ptr<Engine::CameraComponent> SceneView::GetMainCamera() const
	{
		C_P(SceneView);
		return d->MainCamera;
	}

	std::vector<std::shared_ptr<Actor>>& SceneView::GetAllActors() const
	{
		C_P(SceneView);
		return d->Actors;
	}

	const std::vector<Light>& SceneView::GetLights() const
	{
		C_P(const SceneView);
		return d->lightInfos;
	}

	std::vector<Light>& SceneView::GetLights()
	{
		C_P(SceneView);
		return d->lightInfos;
	}

	void SceneView::OnMouseButtonDown(MouseButton Button, core::vec2f Pos)
	{
		C_P(SceneView);
		std::lock_guard Lock(d->DeviceLock);
		HandleMouseEvent(MET_ButtonDown, Button, Pos);
	}

	void SceneView::OnMouseButtonUp(MouseButton Button, core::vec2f Pos)
	{
		C_P(SceneView);
		std::lock_guard Lock(d->DeviceLock);
		HandleMouseEvent(MET_ButtonUp, Button, Pos);
	}

	void SceneView::OnMouseMove(MouseButton Button, core::vec2f Pos)
	{
		C_P(SceneView);
		std::lock_guard Lock(d->DeviceLock);
		HandleMouseEvent(MET_Move, Button,Pos);
	}

	void SceneView::HandleMouseEvent(MouseEventType EventType,MouseButton Button, core::vec2f Pos)
	{
		C_P(SceneView);
		InputDeviceState InputState{};
		InputState.Device = Mouse;
		InputState.MouseInputState.EventType = EventType;
		InputState.MouseInputState.Button = Button;
		InputState.MouseInputState.Pos = Pos;
		d->InputStates.emplace(InputState);
	}

	void SceneView::OnMouseWheel(int32_t WheelValue)
	{
		C_P(SceneView);
		std::lock_guard Lock(d->DeviceLock);
		InputDeviceState InputState{};
		InputState.Device = Mouse;
		InputState.MouseInputState.EventType = MouseEventType::MET_Wheel;
		InputState.MouseInputState.WheelValue = WheelValue;
		d->InputStates.emplace(InputState);
	}

}