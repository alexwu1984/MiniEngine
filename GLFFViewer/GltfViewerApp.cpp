#include "GltfViewerApp.h"
#include "Engine/Scene/GltfOrbitActor.h"
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
	std::wstring ModelFile = Path.wstring() + L"/GLTFModel/huojian.glb";
	auto Scene = Engine::GEngine->GetScene();
	GltfActor = std::make_shared<Engine::GltfOrbitActor>(Scene, ModelFile);
	GltfActor->InitResouce();
	return true;
}

void GltfViewApp::ShutDown()
{
	GltfActor = {};
}
