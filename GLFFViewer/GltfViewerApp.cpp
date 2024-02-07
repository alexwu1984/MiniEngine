#include "GltfViewerApp.h"
#include "Engine/Scene/GltfActor.h"
#include "Engine/Engine.h"
#include "Engine/Scene/SceneView.h"
#include "core/system.h"
#include "Scene/CameraComponent.h"

GltfViewApp::GltfViewApp()
{

}

GltfViewApp::~GltfViewApp()
{

}

bool GltfViewApp::Init()
{
	core::filesystem::path Path = core::process_directory();
	std::wstring ModelFile = Path.wstring() + L"/GLTFModel/Model3.json";
	
	auto Scene = Engine::GEngine->GetScene();
	Scene->LoadScene(ModelFile);

	auto Camera = Scene->GetMainCamera();
	if (Camera)
	{
		auto CameraPos = Camera->GetCameraPos();
		CameraPos.y = 1;
		Camera->SetCameraPos(CameraPos);
	}

	return true;
}

void GltfViewApp::ShutDown()
{
	AGltfModel = {};
}
