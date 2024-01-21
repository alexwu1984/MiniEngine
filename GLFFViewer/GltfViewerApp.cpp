#include "GltfViewerApp.h"
#include "Engine/Scene/GltfActor.h"
#include "Engine/Engine.h"
#include "Engine/Scene/SceneView.h"
#include "core/system.h"

GltfViewApp::GltfViewApp()
{

}

GltfViewApp::~GltfViewApp()
{

}

bool GltfViewApp::Init()
{
	core::filesystem::path Path = core::process_directory();
	//std::wstring ModelFile = Path.wstring() + L"/GLTFModel/Model2.json";
	//std::wstring ModelFile = Path.wstring() + L"/GLTFModel/Model4.json";
	std::wstring ModelFile = Path.wstring() + L"/GLTFModel/Model3.json";
	
	auto Scene = Engine::GEngine->GetScene();
	Scene->LoadScene(ModelFile);

	return true;
}

void GltfViewApp::ShutDown()
{
	AGltfModel = {};
}
