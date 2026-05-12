#include "GltfViewerApp.h"
#include "Engine/Scene/GltfActor.h"
#include "Engine/Engine.h"
#include "Engine/Scene/World.h"
#include "Scene/SkyLightComponent.h"
#include "Engine/Scene/DirectionalLightComponent.h"
#include "Scene/PointLightComponent.h"
#include "Scene/SpotLightComponent.h"
#include "Scene/SceneMeshComponent.h"
#include "core/system.h"
#include "Scene/CameraComponent.h"
#include "App/AppWindow.h"
#include "Render/WorldSceneRender.h"
#include "Render/MaterialPreFrame.h"
#include "RHI/DynamicRHI.h"
#include "Imgui/imgui.h"
#include "core/strings.h"
#include "math/vector3.h"
#include "Render/RDGBuilder.h"
#include "core/logger.h"
#include "core/wall_timer.h"
#include <cmath>

using namespace Engine;

namespace
{
	static bool ActorHostsSceneMesh(const std::shared_ptr<Actor>& Owner)
	{
		if (!Owner)
			return false;
		for (const auto& c : Owner->GetAllComponents())
			if (ComponentCast<SceneMeshComponent>(c))
				return true;
		return false;
	}

	static std::string ActorNameOrFallbackUtf8(const std::shared_ptr<Actor>& Owner, const char* FallbackPrefix, int Idx)
	{
		if (!Owner)
			return std::string(FallbackPrefix) + " #" + std::to_string(Idx);
		const std::wstring& n = Owner->GetActorName();
		if (!n.empty())
			return core::ucs2_u8(n);
		return std::string(FallbackPrefix) + " #" + std::to_string(Idx);
	}

	static void ConeAxisToYawPitchDeg(const math::Vector3& coneIn, float& yawDeg, float& pitchDeg)
	{
		math::Vector3 c = coneIn;
		if (c.GetSqrLength() < 1e-10f)
			c = math::Vector3(0.f, 0.f, 1.f);
		else
			c = c.Normalize();
		pitchDeg = std::asin((std::max)(-1.f, (std::min)(1.f, c.y))) * (180.f / 3.14159265f);
		yawDeg = std::atan2(c.x, c.z) * (180.f / 3.14159265f);
	}

	static math::Vector3 YawPitchDegToConeAxis(float yawDeg, float pitchDeg)
	{
		const float kPi = 3.14159265f;
		const float yr = yawDeg * (kPi / 180.f);
		const float pr = pitchDeg * (kPi / 180.f);
		const float cy = std::cos(yr), sy = std::sin(yr);
		const float cp = std::cos(pr), sp = std::sin(pr);
		math::Vector3 v(sy * cp, sp, cy * cp);
		if (v.GetSqrLength() < 1e-10f)
			return math::Vector3(0.f, 0.f, 1.f);
		return v.Normalize();
	}

	static float SpotCosToHalfAngleDeg(float cosHalfAngle)
	{
		const float c = (std::max)(-1.f, (std::min)(1.f, cosHalfAngle));
		return std::acos(c) * (180.f / 3.14159265f);
	}

	static float SpotHalfAngleDegToCos(float halfAngleDeg)
	{
		halfAngleDeg = (std::max)(0.f, (std::min)(89.f, halfAngleDeg));
		const float rad = halfAngleDeg * (3.14159265f / 180.f);
		return std::cos(rad);
	}
}

GltfViewApp::GltfViewApp()
{
	
}

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

	auto Scene = Engine::GEngine ? Engine::GEngine->GetWorld() : nullptr;
	if (!Scene)
		return false;

	auto Camera = Scene->GetMainCamera();
	if (Camera)
	{
		auto CameraPos = Camera->GetCameraPos();
		Camera->SetCameraPos(CameraPos);
	}
	const double MsCameraTouch = Wall.split_ms();

	if (Engine::GEngine)
	{
		Engine::GEngine->SetEndFrameTickCallback([this]() { FlushPendingModelReload(); });
	}
	const double MsEndFrameCallback = Wall.split_ms();

	const double MsTotal = Wall.total_ms();
	// reload_scene_ms includes ReloadSceneJson + BindImGuiToSceneRender (see ReloadScene).
	core::inf() << core::perf::hdr(core::perf::kBoot, "GltfViewerInit") << "total_ms=" << MsTotal << " build_model_list_ms=" << MsBuildList << " reload_scene_ms=" << MsReloadScene
				<< " camera_touch_ms=" << MsCameraTouch << " end_frame_callback_ms=" << MsEndFrameCallback << "\n";

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
	sr->sigGuiEvent.unbind(this);
	sr->sigGuiEvent.bind(
		[this] {
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

				ImGui::Separator();
				ImGui::TextUnformatted("Viewport");
				{
					bool bShowBounds = Scene->GetShowSceneMeshBoundsDebug();
					if (ImGui::Checkbox("Show model bounds (UE-style)", &bShowBounds))
						Scene->SetShowSceneMeshBoundsDebug(bShowBounds);
				}

				ImGui::Separator();
				ImGui::TextUnformatted("Scene light components");

				const std::vector<std::shared_ptr<DirectionalLightComponent>> dirs =
					Scene->GetDirectionalLightsForEditingSorted();
				const std::vector<std::shared_ptr<PointLightComponent>> pts = Scene->GetPointLightsForEditingSorted();
				const std::vector<std::shared_ptr<SpotLightComponent>> spts = Scene->GetSpotLightsForEditingSorted();
				if (dirs.empty() && pts.empty() && spts.empty())
					ImGui::TextDisabled("(No DirectionalLight / PointLight / SpotLight components)");

				for (int i = 0; i < (int)dirs.size(); ++i)
				{
					const auto& dir = dirs[(size_t)i];
					const auto Owner = dir ? dir->GetOwner() : nullptr;
					ImGui::PushID(100000 + i);
					const std::string titleDir = std::string("Directional:") + ActorNameOrFallbackUtf8(Owner, "DirectionalLight", i);
					if (ImGui::TreeNodeEx(titleDir.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
					{
						dir->SetUseActorForward(false);

						math::Vector3 d = dir->GetWorldDirection().Normalize();
						float yawDeg = 0.f, pitchDeg = 0.f;
						ConeAxisToYawPitchDeg(d, yawDeg, pitchDeg);
						const bool chYaw = ImGui::DragFloat("Yaw deg (world toward source)", &yawDeg, 0.5f, -180.f, 180.f);
						const bool chPitch = ImGui::DragFloat("Pitch deg (world toward source)", &pitchDeg, 0.5f, -85.f, 85.f);
						if (chYaw || chPitch)
						{
							math::Vector3 nd = YawPitchDegToConeAxis(yawDeg, pitchDeg);
							dir->SetWorldDirection(nd);
							if (auto sl = Scene->FindPrimarySkyLightComponent(); sl && sl->IsEnabled() && sl->IsProceduralSky())
								sl->SetProceduralSunDirectionTowardSource(nd);
						}

						math::Vector3 col = dir->GetColor();
						if (ImGui::DragFloat3("Color RGB (linear)", &col.x, 0.03f, 0.f, 32.f))
							dir->SetColor(col);

						float stren = dir->GetIntensity();
						if (ImGui::DragFloat("Intensity", &stren, 0.1f, 0.f, 50.f))
							dir->SetIntensity((std::max)(stren, 0.f));

						{
							bool bShowSh = dir->GetShowShadowFrustumDebug();
							if (ImGui::Checkbox("Show shadow frustum", &bShowSh))
								dir->SetShowShadowFrustumDebug(bShowSh);
							ImGui::TextDisabled("Draws ortho shadow frustum when this lamp owns directional shadow.");
						}

						ImGui::TreePop();
					}
					ImGui::PopID();
				}

				ImGui::Separator();

				for (int i = 0; i < (int)pts.size(); ++i)
				{
					const auto& pl = pts[(size_t)i];
					const auto Owner = pl ? pl->GetOwner() : nullptr;
					ImGui::PushID(200000 + i);
					const std::string titlePt = std::string("Point: ") + ActorNameOrFallbackUtf8(Owner, "PointLight", i);
					if (ImGui::TreeNodeEx(titlePt.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
					{
						const bool bOnMeshActor = ActorHostsSceneMesh(Owner);

						if (bOnMeshActor)
						{
							math::Vector3 lo = pl->GetLocalOffset();
							if (ImGui::DragFloat3("Local offset", &lo.x, 0.03f))
								pl->SetLocalOffset(lo);
						}
						else if (Owner)
						{
							math::Vector3 wp = Owner->GetPosition();
							if (ImGui::DragFloat3("World position", &wp.x, 0.03f))
								Owner->SetPosition(wp);
						}

						math::Vector3 colp = pl->GetColor();
						if (ImGui::DragFloat3("Color RGB (linear)", &colp.x, 0.03f, 0.f, 32.f))
							pl->SetColor(colp);

						float strenp = pl->GetIntensity();
						if (ImGui::DragFloat("Intensity", &strenp, 1.f, 0.f, 500.f))
							pl->SetIntensity((std::max)(strenp, 0.f));

						float rng = pl->GetRange();
						if (ImGui::DragFloat("Attenuation range", &rng, 0.25f, 0.05f, 256.f))
							pl->SetRange((std::max)(rng, 0.01f));

						bool bCastPt = pl->GetCastShadow();
						if (ImGui::Checkbox("Cast shadow", &bCastPt))
							pl->SetCastShadow(bCastPt);
						{
							bool bShowSh = pl->GetShowShadowFrustumDebug();
							if (ImGui::Checkbox("Show shadow bounds", &bShowSh))
								pl->SetShowShadowFrustumDebug(bShowSh);
							ImGui::TextDisabled("Draws cube shadow bounds when this lamp owns cubemap shadow.");
						}

						ImGui::TreePop();
					}
					ImGui::PopID();
				}

				for (int i = 0; i < (int)spts.size(); ++i)
				{
					const auto& sp = spts[(size_t)i];
					const auto Owner = sp ? sp->GetOwner() : nullptr;
					ImGui::PushID(300000 + i);
					const std::string titleSp = std::string("Spot: ") + ActorNameOrFallbackUtf8(Owner, "SpotLight", i);
					if (ImGui::TreeNodeEx(titleSp.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
					{
						if (sp->IsProceduralSunFill())
						{
							float dist = sp->GetProceduralSunDistanceAlongRay();
							if (ImGui::DragFloat("Sun distance along ray", &dist, 1.f, 1.f, 2000.f))
								sp->SetProceduralPlacement(dist, sp->GetProceduralAimWorld());
							math::Vector3 aim = sp->GetProceduralAimWorld();
							if (ImGui::DragFloat3("Aim (world)", &aim.x, 0.05f))
								sp->SetProceduralPlacement(sp->GetProceduralSunDistanceAlongRay(), aim);
						}
						else if (Owner)
						{
							math::Vector3 wp = Owner->GetPosition();
							if (ImGui::DragFloat3("World position", &wp.x, 0.03f))
								Owner->SetPosition(wp);
							float yawDeg = 0.f, pitchDeg = 0.f;
							ConeAxisToYawPitchDeg(sp->GetConeAxisWorld(), yawDeg, pitchDeg);
							const bool yawCh = ImGui::DragFloat("Yaw deg (cone axis)", &yawDeg, 0.5f, -180.f, 180.f);
							const bool pitchCh = ImGui::DragFloat("Pitch deg (cone axis)", &pitchDeg, 0.5f, -85.f, 85.f);
							if (yawCh || pitchCh)
								sp->SetConeAxisWorld(YawPitchDegToConeAxis(yawDeg, pitchDeg));
						}

						math::Vector3 cols = sp->GetColor();
						if (ImGui::DragFloat3("Color RGB (linear)", &cols.x, 0.03f, 0.f, 32.f))
							sp->SetColor(cols);

						float st = sp->GetIntensity();
						if (ImGui::DragFloat("Intensity", &st, 0.25f, 0.f, 200.f))
							sp->SetIntensity((std::max)(st, 0.f));

						float rngs = sp->GetRange();
						if (ImGui::DragFloat("Range (<0 unlimited)", &rngs, 0.5f, -1.f, 2000.f))
							sp->SetRange(rngs);

						bool bCastSh = sp->GetCastShadow();
						if (ImGui::Checkbox("Cast shadow", &bCastSh))
							sp->SetCastShadow(bCastSh);
						{
							bool bShowSf = sp->GetShowShadowFrustumDebug();
							if (ImGui::Checkbox("Show shadow frustum", &bShowSf))
								sp->SetShowShadowFrustumDebug(bShowSf);
							ImGui::TextDisabled("Draws pyramid when this lamp owns spot shadow map.");
						}

						float innerDeg = SpotCosToHalfAngleDeg(sp->GetInnerConeCos());
						float outerDeg = SpotCosToHalfAngleDeg(sp->GetOuterConeCos());
						const bool innerCh = ImGui::DragFloat("Inner half-angle (deg)", &innerDeg, 0.5f, 0.f, 89.f);
						const bool outerCh = ImGui::DragFloat("Outer half-angle (deg)", &outerDeg, 0.5f, 1.f, 89.f);
						if (innerCh || outerCh)
						{
							outerDeg = (std::max)(1.f, (std::min)(89.f, outerDeg));
							innerDeg = (std::max)(0.f, (std::min)(89.f, innerDeg));
							if (innerDeg > outerDeg)
								innerDeg = outerDeg;
							sp->SetOuterConeCos(SpotHalfAngleDegToCos(outerDeg));
							sp->SetInnerConeCos(SpotHalfAngleDegToCos(innerDeg));
						}

						ImGui::TreePop();
					}
					ImGui::PopID();
				}

				ImGui::Separator();
				ImGui::TextUnformatted("SkyLight / IBL");
				ImGui::TextDisabled("HDR rotation removed (default orbit camera).");

				ImGui::Separator();
				ImGui::TextUnformatted("Scene model transform");
				{
					const auto actors = Scene->GetAllActors();
					int modelUiIdx = 0;
					for (const auto& a : actors)
					{
						if (!a)
							continue;
						const auto sm = a->GetComponent<SceneMeshComponent>();
						if (!sm || !sm->IsRotationEditableInUi())
							continue;
						ImGui::PushID(400000 + modelUiIdx);
						++modelUiIdx;
						const std::string title = std::string("Model: ") + ActorNameOrFallbackUtf8(a, "Model", modelUiIdx - 1);
						if (ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
						{
							math::Vector3 wp = a->GetPosition();
							if (ImGui::DragFloat3("World position", &wp.x, 0.03f))
								a->SetPosition(wp);

							float yawDeg = 0.f, pitchDeg = 0.f;
							ConeAxisToYawPitchDeg(a->GetForward(), yawDeg, pitchDeg);
							const bool yawCh = ImGui::DragFloat("Yaw deg (model forward)", &yawDeg, 0.5f, -180.f, 180.f);
							const bool pitchCh = ImGui::DragFloat("Pitch deg (model forward)", &pitchDeg, 0.5f, -85.f, 85.f);
							if (yawCh || pitchCh)
							{
								a->RotateToNewForward(YawPitchDegToConeAxis(yawDeg, pitchDeg));
								a->ComputeWorldTransform(0.f);
							}

							ImGui::TreePop();
						}
						ImGui::PopID();
					}
					if (modelUiIdx == 0)
						ImGui::TextDisabled("(No models with EnableRotationUI=true)");
				}
			}

			ImGui::End();

			ImGui::SetNextWindowPos(ImVec2(380, 1), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(480, 360), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Pipeline", nullptr))
			{
				const ImGuiIO& io = ImGui::GetIO();
				ImGui::Text("%.1f FPS   %.3f ms/frame", io.Framerate, static_cast<double>(io.DeltaTime * 1000.0f));
				ImGui::Separator();

				auto srPipe = Engine::GEngine ? Engine::GEngine->GetSceneRender() : nullptr;
				if (Engine::GEngine)
				{
					const char* apiLabel =
						Engine::GEngine->GetInitRHIApiType() == RenderCore::RHIAPIType::E_D3D11 ? "D3D11" : "D3D12";
					ImGui::TextUnformatted(apiLabel);
				}
				ImGui::TextDisabled("CPU: render-thread wall clock in Execute(). GPU: prior-frame segment (timestamp queries).");
				if (srPipe)
				{
					std::vector<Engine::FRDGPassCpuTiming> rows;
					srPipe->GetLastFramePassCpuTimings(rows);
					double sumCpu = 0.0;
					double sumGpu = 0.0;
					for (const auto& r : rows)
					{
						sumCpu += r.MsCpu;
						if (r.MsGpu >= 0.0)
							sumGpu += r.MsGpu;
					}
					ImGui::Text("RDG sum CPU: %.3f ms   GPU (prev frame): %.3f ms", sumCpu, sumGpu);
					// Fixed-column rows (no BeginTable): nested table clip rects were misaligned with our DX11/DX12 present path.
					const float colCpu = 260.f;
					const float colGpu = 360.f;
					ImGui::Separator();
					ImGui::TextUnformatted("Pass");
					ImGui::SameLine(colCpu);
					ImGui::TextUnformatted("ms CPU");
					ImGui::SameLine(colGpu);
					ImGui::TextUnformatted("ms GPU");
					ImGui::Separator();
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 3.f));
					for (const auto& r : rows)
					{
						ImGui::TextUnformatted(r.Name.c_str());
						ImGui::SameLine(colCpu);
						ImGui::Text("%.3f", r.MsCpu);
						ImGui::SameLine(colGpu);
						if (r.MsGpu >= 0.0)
							ImGui::Text("%.3f", r.MsGpu);
						else
							ImGui::TextDisabled("-");
					}
					ImGui::PopStyleVar();
				}
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

	// HDR rotation removed.
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
