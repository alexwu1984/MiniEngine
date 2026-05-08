#include "GltfViewerApp.h"
#include "Engine/Scene/GltfActor.h"
#include "Engine/Engine.h"
#include "Engine/Scene/World.h"
#include "Scene/SkyLightComponent.h"
#include "Engine/Scene/DirectionalLightComponent.h"
#include "Scene/PointLightComponent.h"
#include "Scene/SceneMeshComponent.h"
#include "core/system.h"
#include "Scene/CameraComponent.h"
#include "App/AppWindow.h"
#include "Render/WorldSceneRender.h"
#include "Render/MaterialPreFrame.h"
#include "RHI/DynamicRHI.h"
#include "Imgui/imgui.h"
#include "core/commandline.h"
#include "core/strings.h"
#include "math/matrix4x4.h"
#include "math/vector3.h"
#include <cfloat>
#include "Render/RDGBuilder.h"
#include "RHI/DynamicRHI.h"
#include <algorithm>
#include <cmath>

namespace
{
	static bool ProjectWorldToImGuiScreen(const math::Vector3& w, const math::Matrix4x4& viewProj, ImVec2& out)
	{
		const math::Vector4 p(w.x, w.y, w.z, 1.f);
		const math::Vector4 c = p * viewProj;
		if (std::fabs(c.w) < 1e-6f)
			return false;
		const float iw = 1.f / c.w;
		const float ndcX = c.x * iw;
		const float ndcY = c.y * iw;
		const ImGuiIO& io = ImGui::GetIO();
		out.x = (ndcX * 0.5f + 0.5f) * io.DisplaySize.x;
		out.y = (1.f - (ndcY * 0.5f + 0.5f)) * io.DisplaySize.y;
		return true;
	}

	/** Same clip-space box as AMD glTFSample WireframeBox + Renderer.cpp light frustum: GenerateBox verts scaled by
	 *  vCenter=(0,0,0.5) vRadius=(1,1,0.5), world = clipLight * inverse(LightViewProj) (row-vector clip = world * LightViewProj). */
	static void DrawDirLightOrthoFrustumOverlay(ImDrawList* dl, const math::Matrix4x4& lightViewProj, const math::Vector3& lightDirTowardSource,
												const math::Matrix4x4& cameraViewProj, ImU32 col)
	{
		static const int edges[12][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
										   { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
		static const float rawBox[8][3] = {
			{ -1.f, -1.f, 1.f }, { 1.f, -1.f, 1.f }, { 1.f, 1.f, 1.f }, { -1.f, 1.f, 1.f },
			{ -1.f, -1.f, -1.f }, { 1.f, -1.f, -1.f }, { 1.f, 1.f, -1.f }, { -1.f, 1.f, -1.f },
		};
		constexpr float cx = 0.f, cy = 0.f, cz = 0.5f;
		constexpr float rx = 1.f, ry = 1.f, rz = 0.5f;

		const float detVp = lightViewProj.Determinant();
		if (std::fabs(detVp) < 1e-24f)
			return;
		const math::Matrix4x4 invLP = lightViewProj.Inverse();
		math::Vector3 c[8]{};
		for (int i = 0; i < 8; ++i)
		{
			const float lx = cx + rawBox[i][0] * rx;
			const float ly = cy + rawBox[i][1] * ry;
			const float lz = cz + rawBox[i][2] * rz;
			const math::Vector4 clipLight(lx, ly, lz, 1.f);
			const math::Vector4 worldH = clipLight * invLP;
			const float w = worldH.w;
			if (std::fabs(w) < 1e-8f)
				c[i] = math::Vector3(worldH.x, worldH.y, worldH.z);
			else
			{
				const float iw = 1.f / w;
				c[i] = math::Vector3(worldH.x * iw, worldH.y * iw, worldH.z * iw);
			}
		}
		ImVec2 s[8]{};
		unsigned mask = 0;
		for (int i = 0; i < 8; ++i)
		{
			if (ProjectWorldToImGuiScreen(c[i], cameraViewProj, s[i]))
				mask |= (1u << i);
		}
		for (const auto& e : edges)
		{
			if ((mask & (1u << e[0])) && (mask & (1u << e[1])))
				dl->AddLine(s[e[0]], s[e[1]], col, 2.f);
		}

		math::Vector3 center(0.f, 0.f, 0.f);
		for (const auto& p : c)
			center = center + p;
		center = center * (1.f / 8.f);

		math::Vector3 mn(FLT_MAX, FLT_MAX, FLT_MAX);
		math::Vector3 mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (const auto& p : c)
		{
			mn.x = (std::min)(mn.x, p.x);
			mn.y = (std::min)(mn.y, p.y);
			mn.z = (std::min)(mn.z, p.z);
			mx.x = (std::max)(mx.x, p.x);
			mx.y = (std::max)(mx.y, p.y);
			mx.z = (std::max)(mx.z, p.z);
		}
		const float diag = (mx - mn).GetLength();
		const float axisHalf = (std::max)(diag * 0.55f, 0.35f);

		const math::Vector3 dir = lightDirTowardSource.Normalize();
		if (dir.GetSqrLength() < 1e-12f)
			return;

		const math::Vector3 sunW = center + dir * axisHalf;
		const math::Vector3 shadowW = center - dir * axisHalf;

		ImVec2 pSun{}, pSh{};
		if (ProjectWorldToImGuiScreen(sunW, cameraViewProj, pSun) && ProjectWorldToImGuiScreen(shadowW, cameraViewProj, pSh))
		{
			const ImU32 axisCol = IM_COL32(60, 240, 255, 255);
			dl->AddLine(pSh, pSun, axisCol, 3.f);

			float vx = pSun.x - pSh.x;
			float vy = pSun.y - pSh.y;
			const float len = std::sqrt(vx * vx + vy * vy);
			if (len > 6.f)
			{
				vx /= len;
				vy /= len;
				const float ah = 16.f;
				const float hw = 7.f;
				const ImVec2 tip = pSun;
				const ImVec2 base(pSun.x - vx * ah, pSun.y - vy * ah);
				const ImVec2 perp(-vy * hw, vx * hw);
				dl->AddTriangleFilled(tip, ImVec2(base.x + perp.x, base.y + perp.y), ImVec2(base.x - perp.x, base.y - perp.y), axisCol);
			}
		}
	}
}

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
		L"Model1.json",
		L"Model3.json",
		L"Model5.json",
		L"Model4.json",
		L"harley.json",
		L"busterDrone.json",
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

				ImGui::Separator();
				ImGui::TextUnformatted("Scene light components");

				const std::vector<std::shared_ptr<DirectionalLightComponent>> dirs =
					Scene->GetDirectionalLightsForEditingSorted();
				const std::vector<std::shared_ptr<PointLightComponent>> pts = Scene->GetPointLightsForEditingSorted();
				if (dirs.empty() && pts.empty())
					ImGui::TextDisabled("(No DirectionalLight / PointLight components)");

				for (int i = 0; i < (int)dirs.size(); ++i)
				{
					const auto& dir = dirs[(size_t)i];
					const auto Owner = dir ? dir->GetOwner() : nullptr;
					ImGui::PushID(100000 + i);
					const std::string titleDir = std::string("Directional:") + ActorNameOrFallbackUtf8(Owner, "DirectionalLight", i);
					if (ImGui::TreeNodeEx(titleDir.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
					{
						dir->SetUseActorForward(false);

						math::Vector3 d = dir->GetWorldDirection();
						if (ImGui::DragFloat3("Direction (world)", &d.x, 0.02f))
						{
							d.Normalize();
							if (d.GetSqrLength() >= 1e-10f)
								dir->SetWorldDirection(d);
						}

						math::Vector3 col = dir->GetColor();
						if (ImGui::DragFloat3("Color RGB (linear)", &col.x, 0.03f, 0.f, 32.f))
							dir->SetColor(col);

						float stren = dir->GetIntensity();
						if (ImGui::DragFloat("Intensity", &stren, 0.1f, 0.f, 50.f))
							dir->SetIntensity((std::max)(stren, 0.f));

						ImGui::TreePop();
					}
					ImGui::PopID();
				}

				ImGui::Separator();
				if (auto srDbg = Engine::GEngine ? Engine::GEngine->GetSceneRender() : nullptr)
				{
					bool showFrustum = srDbg->GetShowDirectionalLightFrustum();
					if (ImGui::Checkbox("Show directional shadow ortho volume (screen overlay)", &showFrustum))
						srDbg->SetShowDirectionalLightFrustum(showFrustum);
					if (showFrustum)
						ImGui::TextWrapped(
							"Yellow box matches AMD glTFSample WireframeBox light frustum (inverse(LightViewProj) × NDC box "
							"vCenter/radius). Cyan arrow: Light.Direction (toward source). Screen projection still uses the scene "
							"perspective camera.");
				}

				for (int i = 0; i < (int)pts.size(); ++i)
				{
					const auto& pl = pts[(size_t)i];
					const auto Owner = pl ? pl->GetOwner() : nullptr;
					ImGui::PushID(200000 + i);
					const std::string titlePt = std::string("Point: ") + ActorNameOrFallbackUtf8(Owner, "PointLight", i);
					if (ImGui::TreeNodeEx(titlePt.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
					{
						const bool bOnMeshActor = ActorHostsSceneMesh(Owner);

						ImGui::TextUnformatted(bOnMeshActor
												   ? "Mesh host: edit local offset (follows actor transform)."
												   : "Dedicated light actor: edit world position.");

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

						ImGui::TreePop();
					}
					ImGui::PopID();
				}

				ImGui::Separator();
				ImGui::TextUnformatted("SkyLight / IBL");
				ImGui::TextDisabled("HDR rotation removed (default orbit camera).");
			}

			ImGui::End();

			ImGui::SetNextWindowPos(ImVec2(380, 1), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Pipeline", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
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
					if (ImGui::BeginTable("rdg", 3, ImGuiTableFlags_BordersInnerV))
					{
						ImGui::TableSetupColumn("Pass");
						ImGui::TableSetupColumn("ms CPU");
						ImGui::TableSetupColumn("ms GPU");
						ImGui::TableHeadersRow();
						for (const auto& r : rows)
						{
							ImGui::TableNextRow();
							ImGui::TableNextColumn();
							ImGui::TextUnformatted(r.Name.c_str());
							ImGui::TableNextColumn();
							ImGui::Text("%.3f", r.MsCpu);
							ImGui::TableNextColumn();
							if (r.MsGpu >= 0.0)
								ImGui::Text("%.3f", r.MsGpu);
							else
								ImGui::TextDisabled("—");
						}
						ImGui::EndTable();
					}
				}
			}
			ImGui::End();

			if (auto srOv = Engine::GEngine ? Engine::GEngine->GetSceneRender() : nullptr)
			{
				if (srOv->GetShowDirectionalLightFrustum())
				{
					math::Matrix4x4 lvp;
					math::Vector3 lightDir;
					math::Matrix4x4 cvp;
					if (srOv->TryGetGuiDebugDirLightFrustum(lvp, lightDir, cvp))
						DrawDirLightOrthoFrustumOverlay(ImGui::GetForegroundDrawList(), lvp, lightDir, cvp, IM_COL32(255, 220, 64, 255));
				}
			}
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
