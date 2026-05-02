#include "GltfViewerApp.h"
#include "Engine/Scene/GltfActor.h"
#include "Engine/Engine.h"
#include "Engine/Scene/World.h"
#include "Scene/SkyLightComponent.h"
#include "Engine/Scene/DirectionalLightComponent.h"
#include "core/system.h"
#include "Scene/CameraComponent.h"
#include "App/AppWindow.h"
#include "Render/WorldSceneRender.h"
#include "Render/MaterialPreFrame.h"
#include "RHI/DynamicRHI.h"
#include "Imgui/imgui.h"
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
	BuildModelList();
	SelIndex = 0;
	ReloadScene(SelIndex);

	auto Scene = Engine::GEngine ? Engine::GEngine->GetWorld() : nullptr;
	if (!Scene)
		return false;

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
	if (const auto sl = Scene->FindPrimarySkyLightComponent())
	{
		xHDRRotate = sl->GetIBLRotationPitchDegrees();
		yHDRRotate = sl->GetIBLRotationYawDegrees();
	}

	if (Engine::GEngine)
	{
		Engine::GEngine->SetEndFrameTickCallback([this]() { FlushPendingModelReload(); });
	}

	return true;
}

void GltfViewApp::BuildModelList()
{
	ModelFiles.clear();
	ModelLabelsUtf8.clear();

	const core::filesystem::path gltfDir = core::filesystem::path(ProcessDir) / "GLTFModel";
	const std::vector<std::wstring> rel = {
		L"BS_Model5.json",
		L"Model3.json",
		L"Model5.json",
		L"Model4.json",
		L"harley.json",
		L"Model1.json",
		L"Model2.json",
		L"old_bicycle.json",
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

void GltfViewApp::BindImGuiToSceneRender()
{
	if (!Engine::GEngine)
		return;
	auto sr = Engine::GEngine->GetSceneRender();
	if (!sr)
		return;
	sr->sigGuiEvent.unbind(this);
	sr->sigGuiEvent.bind(
		[this] {
			if (core::CommandLine::Get().GetName("noimgui"))
				return;

			auto Scene = Engine::GEngine ? Engine::GEngine->GetWorld() : nullptr;
			if (!Scene)
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
								PendingModelIndex.store(i, std::memory_order_release);
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

				if (const auto sl = Scene->FindPrimarySkyLightComponent())
				{
					ImGui::SliderFloat("xHDRRotate", &xHDRRotate, -180, 180);
					ImGui::SliderFloat("yHDRRotate", &yHDRRotate, -180, 180);
					sl->SetIBLRotationPitchDegrees(xHDRRotate);
					sl->SetIBLRotationYawDegrees(yHDRRotate);
				}
				else
				{
					ImGui::TextUnformatted("No SkyLight (IBL rotation disabled)");
				}
			}

			ImGui::End();

			ImGui::SetNextWindowPos(ImVec2(1, 260), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Perf", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				const ImGuiIO& io = ImGui::GetIO();
				ImGui::Text("%.1f FPS   %.3f ms/frame", io.Framerate, static_cast<double>(io.DeltaTime * 1000.0f));
				if (auto srPerf = Engine::GEngine->GetSceneRender())
				{
					ImGui::Separator();
					ImGui::Text("SubmitSeq (CPU): %llu",
								static_cast<unsigned long long>(srPerf->GetSubmissionSequence()));
					ImGui::Text("Pending ExecFrame: %u", srPerf->GetPendingSceneFramesCount());
					const uint32_t cap = srPerf->GetMaxSceneFramesInFlight();
					if (cap == 0u)
						ImGui::TextUnformatted("maxrenderframes cap: unlimited");
					else
						ImGui::Text("maxrenderframes cap: %u", cap);
				}
				if (Engine::GEngine)
					if (auto rh = Engine::GEngine->GetRHI())
						ImGui::Text("RHI slot hint: %u", rh->RHIRecommendedParallelFrameResourceSlots());
				ImGui::TextDisabled("SubmitSeq is enqueue ordinal, not GPU completion.");
			}
			ImGui::End();
		},
		this);
}

void GltfViewApp::FlushPendingModelReload()
{
	const int32_t NewIndex = PendingModelIndex.exchange(-1, std::memory_order_acq_rel);
	if (NewIndex < 0)
		return;
	if (NewIndex == SelIndex)
		return;
	ReloadScene(NewIndex);
}

void GltfViewApp::ReloadScene(int32_t NewIndex)
{
	if (!Engine::GEngine)
		return;
	if (NewIndex < 0 || NewIndex >= (int32_t)ModelFiles.size())
		return;

	SelIndex = NewIndex;
	Engine::GEngine->ReloadSceneJson(ModelFiles[(size_t)SelIndex]);
	BindImGuiToSceneRender();

	auto Scene = Engine::GEngine->GetWorld();
	if (!Scene)
		return;

	// Refresh cached light direction for UI.
	{
		const auto mergedLights = Scene->GatherLightsForView();
		if (!mergedLights.empty())
			mDirectLight = mergedLights[0].Direction;
	}
	if (const auto sl = Scene->FindPrimarySkyLightComponent())
	{
		xHDRRotate = sl->GetIBLRotationPitchDegrees();
		yHDRRotate = sl->GetIBLRotationYawDegrees();
	}
}

void GltfViewApp::ShutDown()
{
	if (Engine::GEngine)
	{
		Engine::GEngine->SetEndFrameTickCallback({});
		if (auto sr = Engine::GEngine->GetSceneRender())
			sr->sigGuiEvent.unbind(this);
	}
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
