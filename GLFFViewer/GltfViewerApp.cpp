#include "core/inc.h"
#include "GltfViewerApp.h"
#include "GltfViewerEditorPanel.h"
#include "Engine/Engine.h"
#include "Engine/Scene/Actor.h"
#include "Engine/Scene/World.h"
#include "core/system.h"
#include "Scene/CameraComponent.h"
#include "Render/WorldSceneRender.h"
#include "core/logger.h"
#include "core/wall_timer.h"

GltfViewApp::GltfViewApp() = default;

GltfViewApp::~GltfViewApp()
{
	ShutDown();
}

bool GltfViewApp::Init()
{
	core::WallSplitTimer Wall;

	ProcessDir = core::process_directory().wstring();
	BuildModelList();
	const double MsBuildList = Wall.split_ms();
	SelIndex = 0;
	ReloadScene(SelIndex);
	const double MsReloadScene = Wall.split_ms();

	if (auto Scene = Engine::GEngine ? Engine::GEngine->GetWorld() : nullptr)
	{
		if (auto Camera = Scene->GetMainCamera())
		{
			auto CameraPos = Camera->GetCameraPos();
			Camera->SetCameraPos(CameraPos);
		}
	}
	else
		return false;

	const double MsCameraTouch = Wall.split_ms();

	if (Engine::GEngine)
		Engine::GEngine->SetEndFrameTickCallback([this]() { FlushPendingModelReload(); });

	const double MsEndFrameCallback = Wall.split_ms();
	const double MsTotal = Wall.total_ms();
	if (core::perf::ShouldEmitPerfInfLogs())
	{
		core::inf() << core::perf::hdr(core::perf::kBoot, "GltfViewerInit") << "total_ms=" << MsTotal << " build_model_list_ms=" << MsBuildList
					<< " reload_scene_ms=" << MsReloadScene << " camera_touch_ms=" << MsCameraTouch << " end_frame_callback_ms=" << MsEndFrameCallback
					<< "\n";
	}

	return true;
}

void GltfViewApp::BuildModelList()
{
	ModelFiles.clear();
	ModelLabelsUtf8.clear();

	const core::filesystem::path gltfDir = core::filesystem::path(ProcessDir) / "GLTFModel";
	const std::vector<std::wstring> rel = {
		L"mode2_pointlight.json",
		L"busterDrone.json",
		L"harley.json",
		L"BS_Model5.json",
		L"Model_fxaa.json",
		L"spotlight_test.json",
		L"Model4_fur.json",
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

	if (!EditorPanel)
		EditorPanel = std::make_unique<GltfViewerEditorPanel>();

	EditorPanel->SetModelSelection(ModelLabelsUtf8, &SelIndex, &PendingModelIndex);
	EditorPanel->Bind(*sr);
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
	if (NewIndex < 0 || NewIndex >= static_cast<int32_t>(ModelFiles.size()))
		return;

	SelIndex = NewIndex;
	Engine::GEngine->ReloadSceneJson(ModelFiles[static_cast<size_t>(SelIndex)]);
	BindImGuiToSceneRender();
}

void GltfViewApp::ShutDown()
{
	if (Engine::GEngine)
	{
		Engine::GEngine->SetEndFrameTickCallback({});
		if (auto sr = Engine::GEngine->GetSceneRender())
		{
			if (EditorPanel)
				EditorPanel->Unbind(*sr);
		}
	}
	EditorPanel.reset();
	_Demo = {};
	AGltfModel = {};
}

void GltfViewApp::HideActor(const std::wstring& Name)
{
	auto Scene = Engine::GEngine->GetWorld();
	for (const auto& ActorItem : Scene->GetAllActors())
	{
		if (ActorItem && ActorItem->GetActorName() == Name)
		{
			ActorItem->SetVisible(false);
			break;
		}
	}
}

void GltfViewApp::ShowActor(const std::wstring& Name)
{
	auto Scene = Engine::GEngine->GetWorld();
	for (const auto& ActorItem : Scene->GetAllActors())
	{
		if (ActorItem && ActorItem->GetActorName() == Name)
		{
			ActorItem->SetVisible(true);
			break;
		}
	}
}
