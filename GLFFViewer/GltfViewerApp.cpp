#include "GltfViewerApp.h"
#include "Engine/Scene/GltfActor.h"
#include "Engine/Engine.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/DirectionalLightComponent.h"
#include "core/system.h"
#include "Scene/CameraComponent.h"
#include "App/AppWindow.h"
#include "Render/WorldSceneRender.h"
#include "Render/MaterialPreFrame.h"
#include "Imgui/imgui.h"
#include "Thread/RenderThread.h"
#include "core/commandline.h"
#include "core/strings.h"

using namespace Engine;

GltfViewApp::GltfViewApp()
{
	
}

GltfViewApp::~GltfViewApp()
{
	ShutDown();
}

bool GltfViewApp::Init()
{
	ProcessDir = core::process_directory().wstring();
	auto Scene = Engine::GEngine->GetWorld();
	BuildModelList();
	SelIndex = 0;
	ReloadScene(SelIndex);

	auto Camera = Scene->GetMainCamera();
	if (Camera)
	{
		auto CameraPos = Camera->GetCameraPos();
		Camera->SetCameraPos(CameraPos);
	}

	{
		const auto mergedLights = Scene->GatherLightsForView();
		if (!mergedLights.empty())
			mDirectLight = mergedLights[0].Direction;
	}

	if (Engine::GEngine)
	{
		if (auto sr = Engine::GEngine->GetSceneRender())
			sr->sigGuiEvent.unbind(this);
		Engine::GEngine->GetSceneRender()->sigGuiEvent.bind([this, Scene] {
			if (core::CommandLine::Get().GetName("noimgui"))
				return;

		ImGui::SetNextWindowPos(ImVec2(1, 1));
		if (ImGui::Begin("Light", 0, ImGuiWindowFlags_::ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (!ModelLabelsUtf8.empty())
			{
				const char* preview = (SelIndex >= 0 && SelIndex < (int)ModelLabelsUtf8.size()) ? ModelLabelsUtf8[(size_t)SelIndex].c_str() : "";
				if (ImGui::BeginCombo("Model", preview))
				{
					for (int i = 0; i < (int)ModelLabelsUtf8.size(); ++i)
					{
						const bool isSelected = (i == SelIndex);
						if (ImGui::Selectable(ModelLabelsUtf8[(size_t)i].c_str(), isSelected))
							PendingModelIndex = i;
						if (isSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}

			if (const auto dir = Scene->GetPrimaryDirectionalLightForEditing())
			{
				ImGui::SliderFloat("LightDir.x", &mDirectLight.x, -1, 1);
				ImGui::SliderFloat("LightDir.y", &mDirectLight.y, -1, 1);
				ImGui::SliderFloat("LightDir.z", &mDirectLight.z, -1, 1);

				dir->SetUseActorForward(false);
				dir->SetWorldDirection(static_cast<const math::Vector3&>(mDirectLight).Normalize());
			}

			ImGui::SliderFloat("xHDRRotate", &xHDRRotate, -180, 180);
			ImGui::SliderFloat("yHDRRotate", &yHDRRotate, -180, 180);
			Engine::GEngine->GetSceneRender()->SetIBLRotate(xHDRRotate,yHDRRotate);
		}

		ImGui::End();

		if (PendingModelIndex >= 0 && PendingModelIndex != SelIndex)
		{
			const int32_t NewIndex = PendingModelIndex;
			PendingModelIndex = -1;
			ReloadScene(NewIndex);
		}

		}, this);
	}

	return true;
}

void GltfViewApp::BuildModelList()
{
	ModelFiles.clear();
	ModelLabelsUtf8.clear();

	const core::filesystem::path gltfDir = core::filesystem::path(ProcessDir) / "GLTFModel";
	const std::vector<std::wstring> rel = {
		L"Model3.json",
		L"harley.json",
		L"BS_Model5.json",
		L"Model1.json",
		L"Model2.json",
		L"Model5.json",
		L"old_bicycle.json",
		L"Model4.json",
	};

	ModelFiles.reserve(rel.size());
	ModelLabelsUtf8.reserve(rel.size());
	for (const auto& r : rel)
	{
		const core::filesystem::path full = gltfDir / core::filesystem::path(r);
		ModelFiles.push_back(full.wstring());
		ModelLabelsUtf8.push_back(core::ucs2_u8(core::filesystem::path(r).wstring()));
	}
}

void GltfViewApp::ReloadScene(int32_t NewIndex)
{
	auto Scene = Engine::GEngine ? Engine::GEngine->GetWorld() : nullptr;
	if (!Scene)
		return;
	if (NewIndex < 0 || NewIndex >= (int32_t)ModelFiles.size())
		return;

	// Tear down previous scene first.
	Scene->RemoveAllActors();

	SelIndex = NewIndex;
	Scene->LoadScene(ModelFiles[(size_t)SelIndex]);

	// Refresh cached light direction for UI.
	{
		const auto mergedLights = Scene->GatherLightsForView();
		if (!mergedLights.empty())
			mDirectLight = mergedLights[0].Direction;
	}
}

void GltfViewApp::ShutDown()
{
	if (Engine::GEngine)
		if (auto sr = Engine::GEngine->GetSceneRender())
			sr->sigGuiEvent.unbind(this);
	_Demo = {};
	AGltfModel = {};
}

void GltfViewApp::HideActor(const std::wstring& Name)
{
	auto Scene = Engine::GEngine->GetWorld();
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
	auto Scene = Engine::GEngine->GetWorld();
	for (auto ActorItem : Scene->GetAllActors())
	{
		if (ActorItem->GetActorName() == Name)
		{
			ActorItem->SetVisible(true);
			break;
		}
	}
}
