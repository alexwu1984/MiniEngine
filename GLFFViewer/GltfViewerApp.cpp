#include "GltfViewerApp.h"
#include "Engine/Scene/GltfActor.h"
#include "Engine/Engine.h"
#include "Engine/Scene/SceneView.h"
#include "core/system.h"
#include "Scene/CameraComponent.h"
#include "App/AppWindow.h"

GltfViewApp::GltfViewApp()
{

}

GltfViewApp::~GltfViewApp()
{

}

bool GltfViewApp::Init()
{
	core::filesystem::path Path = core::process_directory();
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

	auto AppWindow = Engine::GEngine->GetAppWindow();
	AppWindow->EvtKeyEvent.bind([this, Path](bool is_key_down, int32_t vk, int32_t scancode) {

		switch (vk)
		{
		case '0': 
		{
			if (SelIndex == 0)
				return;
			std::wstring ModelFile = Path.wstring() + L"/GLTFModel/Model4.json";
			SelIndex = 0;
			auto Scene = Engine::GEngine->GetScene();
			Scene->LoadScene(ModelFile);
		}
			break;
		case '1': 
		{
			if (SelIndex == 1)
				return;
			std::wstring ModelFile = Path.wstring() + L"/GLTFModel/Model3.json";
			SelIndex = 1;
			auto Scene = Engine::GEngine->GetScene();
			Scene->LoadScene(ModelFile);
		}
			break;
		default:
			break;
		}
		
	}, this);

	return true;
}

void GltfViewApp::ShutDown()
{
	AGltfModel = {};
}
