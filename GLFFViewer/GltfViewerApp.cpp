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
	std::wstring ModelFile = Path.wstring() + L"/GLTFModel/Model3.json";
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

	//HideActor(L"Model4.glb");
	//HideActor(L"Model2.glb");
	//HideActor(L"floor.glb");

	//auto AppWindow = Engine::GEngine->GetAppWindow();
	//AppWindow->EvtKeyEvent.bind([this, Path, Scene](bool is_key_down, int32_t vk, int32_t scancode) {

	//	switch (vk)
	//	{
	//	case '1': 
	//	{
	//		if (SelIndex == 0)
	//			return;
	//		SelIndex = 0;
	//		HideActor(L"Model4.glb");
	//		HideActor(L"Model2.glb");
	//		ShowActor(L"Model3.glb");

	//	}
	//		break;
	//	case '2': 
	//	{
	//		if (SelIndex == 1)
	//			return;
	//		SelIndex = 1;
	//		HideActor(L"Model3.glb");
	//		HideActor(L"Model2.glb");
	//		ShowActor(L"Model4.glb");
	//	}
	//		break;
	//	case '3':
	//	{
	//		if (SelIndex == 2)
	//			return;
	//		SelIndex = 2;
	//		HideActor(L"Model3.glb");
	//		HideActor(L"Model4.glb");
	//		ShowActor(L"Model2.glb");
	//	}
	//	break;
	//	default:
	//		break;
	//	}
	//	
	//}, this);

	return true;
}

void GltfViewApp::ShutDown()
{
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
