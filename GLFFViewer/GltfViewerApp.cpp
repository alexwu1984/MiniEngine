#include "GltfViewerApp.h"
#include "Engine/Scene/GltfActor.h"
#include "Engine/Engine.h"
#include "Engine/Scene/SceneView.h"
#include "core/system.h"
#include "Scene/CameraComponent.h"
#include "App/AppWindow.h"
#include "Render/SceneRender.h"
#include "Render/MaterialPreFrame.h"
#include "Imgui/imgui.h"
#include "PostProcessDemo.h"
#include "IBLRenderDemo.h"
#include "Thread/RenderThread.h"

using namespace Engine;

GltfViewApp::GltfViewApp()
{
	
}

GltfViewApp::~GltfViewApp()
{

}

bool GltfViewApp::Init()
{
	if (1)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND([this](RenderCore::DynamicRHI* RHI) {
			if (!_Demo)
			{
				_Demo = std::make_shared<IBLRenderDemo>(RHI);
			}
			_Demo->InitResource();
			auto sceneRender = Engine::GEngine->GetSceneRender();
			sceneRender->SetSamplePostProcessor(_Demo);
			});
	}


	if (0)
	{
		core::filesystem::path Path = core::process_directory();
		//std::wstring ModelFile = Path.wstring() + L"/GLTFModel/Model3.json";
		std::wstring ModelFile = Path.wstring() + L"/GLTFModel/Model4.json";
		SelIndex = 0;
		auto Scene = Engine::GEngine->GetScene();
		Scene->LoadScene(ModelFile);

		auto Camera = Scene->GetMainCamera();
		if (Camera)
		{
			auto CameraPos = Camera->GetCameraPos();
			CameraPos.y = 1;
			Camera->SetCameraPos(CameraPos);
		}

		mDirectLight = Scene->GetLights()[0].Direction;

		Engine::GEngine->GetSceneRender()->sigGuiEvent.bind([this, Scene] {

			ImGui::SetNextWindowPos(ImVec2(10, 10));
			if (ImGui::Begin("Light", 0, ImGuiWindowFlags_::ImGuiWindowFlags_AlwaysAutoResize))
			{
				auto& Lights = Scene->GetLights();
				auto& DirectLight = Lights[0];

				ImGui::SliderFloat("LightDir.x", &mDirectLight.x, -1, 1);
				ImGui::SliderFloat("LightDir.y", &mDirectLight.y, -1, 1);
				ImGui::SliderFloat("LightDir.z", &mDirectLight.z, -1, 1);

				DirectLight.Direction = mDirectLight;
				DirectLight.Direction.Normalize();
			}

			ImGui::End();

			}, this);
	}


	return true;
}

void GltfViewApp::ShutDown()
{
	_Demo = {};
	AGltfModel = {};
}

void GltfViewApp::HideActor(const std::wstring& Name)
{
	auto Scene = Engine::GEngine->GetScene();
	for (auto ActorItem : Scene->GetAllActors())
	{
		if (ActorItem->GetActorName() == Name)
		{
			ActorItem->SetVisible(false);
			break;
		}
	}
}

void GltfViewApp::ShowActor(const std::wstring& Name)
{
	auto Scene = Engine::GEngine->GetScene();
	for (auto ActorItem : Scene->GetAllActors())
	{
		if (ActorItem->GetActorName() == Name)
		{
			ActorItem->SetVisible(true);
			break;
		}
	}
}
