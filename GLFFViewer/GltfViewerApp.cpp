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
#include "core/commandline.h"
#include "core/strings.h"
#include "math/matrix4x4.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "Render/RDGBuilder.h"
#include "RHI/DynamicRHI.h"
namespace
{
	/** Perspective divide only for points in front of the camera clip half-space (w>0). */
	static bool ProjectHomogeneousClipToImGuiScreen(const math::Vector4& clip, ImVec2& out)
	{
		static constexpr float kMinW = 1e-3f;
		if (clip.w <= kMinW)
			return false;
		const float iw = 1.f / clip.w;
		const float ndcX = clip.x * iw;
		const float ndcY = clip.y * iw;
		const ImGuiViewport* vp = ImGui::GetMainViewport();
		if (!vp || vp->Size.x <= 1.f || vp->Size.y <= 1.f)
			return false;
		out.x = vp->Pos.x + (ndcX * 0.5f + 0.5f) * vp->Size.x;
		out.y = vp->Pos.y + (1.f - (ndcY * 0.5f + 0.5f)) * vp->Size.y;
		return true;
	}

	/** Clip-space segment clipped to w>=kMinW so edges crossing the eye plane don't mirror to nonsense screen coords. */
	static bool ClipHomogeneousSegmentToMinW(math::Vector4 ha, math::Vector4 hb, math::Vector4& outA, math::Vector4& outB)
	{
		static constexpr float kMinW = 1e-3f;
		const bool inA = ha.w > kMinW;
		const bool inB = hb.w > kMinW;
		if (inA && inB)
		{
			outA = ha;
			outB = hb;
			return true;
		}
		if (!inA && !inB)
			return false;
		if (!inA)
		{
			if (std::fabs(hb.w - ha.w) < 1e-12f)
				return false;
			const float t = (kMinW - ha.w) / (hb.w - ha.w);
			if (t < 0.f || t > 1.f)
				return false;
			outA = ha + (hb - ha) * t;
			outB = hb;
			return true;
		}
		// !inB
		if (std::fabs(ha.w - hb.w) < 1e-12f)
			return false;
		const float t = (kMinW - hb.w) / (ha.w - hb.w);
		if (t < 0.f || t > 1.f)
			return false;
		outA = ha;
		outB = hb + (ha - hb) * t;
		return true;
	}

	static bool ProjectWorldEdgeToImGuiScreen(const math::Vector3& wa, const math::Vector3& wb, const math::Matrix4x4& viewProj, ImVec2& sa, ImVec2& sb)
	{
		const math::Vector4 ha = math::Vector4(wa.x, wa.y, wa.z, 1.f) * viewProj;
		const math::Vector4 hb = math::Vector4(wb.x, wb.y, wb.z, 1.f) * viewProj;
		math::Vector4 ca, cb;
		if (!ClipHomogeneousSegmentToMinW(ha, hb, ca, cb))
			return false;
		return ProjectHomogeneousClipToImGuiScreen(ca, sa) && ProjectHomogeneousClipToImGuiScreen(cb, sb);
	}

	/** Same clip-space box as AMD glTFSample WireframeBox + Renderer.cpp light frustum: GenerateBox verts scaled by
	 *  vCenter=(0,0,0.5) vRadius=(1,1,0.5), world = clipLight * inverse(LightViewProj) (row-vector clip = world * LightViewProj). */
	static void DrawDirLightOrthoFrustumOverlay(ImDrawList* dl, const math::Matrix4x4& lightViewProj, const math::Matrix4x4& cameraViewProj, ImU32 col)
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
		for (const auto& e : edges)
		{
			ImVec2 sa, sb;
			if (ProjectWorldEdgeToImGuiScreen(c[e[0]], c[e[1]], cameraViewProj, sa, sb))
				dl->AddLine(sa, sb, col, 2.f);
		}
	}

	/** Spot shadow frustum wireframe in world space from inverse(LightViewProj) * NDC clip corners. */
	static void DrawSpotShadowFrustumWireOverlay(ImDrawList* dl, const math::Matrix4x4& lightViewProj, const math::Matrix4x4& cameraViewProj,
												 ImU32 wireCol)
	{
		static const int edges[12][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
										   { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
		const float detVp = lightViewProj.Determinant();
		if (std::fabs(detVp) < 1e-24f)
			return;
		const math::Matrix4x4 invLP = lightViewProj.Inverse();
		const math::Vector4 clipCorners[8] = {
			math::Vector4(-1.f, -1.f, 0.f, 1.f), math::Vector4(1.f, -1.f, 0.f, 1.f), math::Vector4(1.f, 1.f, 0.f, 1.f), math::Vector4(-1.f, 1.f, 0.f, 1.f),
			math::Vector4(-1.f, -1.f, 1.f, 1.f), math::Vector4(1.f, -1.f, 1.f, 1.f), math::Vector4(1.f, 1.f, 1.f, 1.f), math::Vector4(-1.f, 1.f, 1.f, 1.f),
		};
		math::Vector3 c[8]{};
		for (int i = 0; i < 8; ++i)
		{
			const math::Vector4 worldH = clipCorners[i] * invLP;
			const float w = worldH.w;
			if (std::fabs(w) < 1e-8f)
				c[i] = math::Vector3(worldH.x, worldH.y, worldH.z);
			else
			{
				const float iw = 1.f / w;
				c[i] = math::Vector3(worldH.x * iw, worldH.y * iw, worldH.z * iw);
			}
		}
		for (const auto& e : edges)
		{
			ImVec2 sa, sb;
			if (ProjectWorldEdgeToImGuiScreen(c[e[0]], c[e[1]], cameraViewProj, sa, sb))
				dl->AddLine(sa, sb, wireCol, 2.f);
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
					if (ImGui::Checkbox("Show light frustum", &showFrustum))
						srDbg->SetShowDirectionalLightFrustum(showFrustum);
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
						else if (Scene->DoesSpotUseProceduralSunKeyInGather(sp))
						{
							if (auto sl = Scene->FindPrimarySkyLightComponent())
							{
								math::Vector3 sunDir = sl->GetProceduralSunDirectionTowardSource();
								if (sunDir.GetSqrLength() < 1e-10f)
									sunDir = math::Vector3(0.f, 0.49f, 0.833f);
								sunDir = sunDir.Normalize();
								const math::Vector3 aim = Scene->ResolveProceduralSunAimWorldForGather(*sp);
								const float dist = sp->GetProceduralSunDistanceAlongRay();
								const math::Vector3 pos = aim + sunDir * dist;
								const math::Vector3 toAim = aim - pos;
								math::Vector3 axisToScene =
									toAim.GetSqrLength() >= 1e-10f ? toAim.Normalize() : (-sunDir);
								ImGui::Text("Effective position (Gather): %.3f %.3f %.3f", pos.x, pos.y, pos.z);
								ImGui::Text("Effective cone axis toward scene: %.3f %.3f %.3f", axisToScene.x, axisToScene.y, axisToScene.z);
							}
						}
						else if (Owner)
						{
							math::Vector3 wp = Owner->GetPosition();
							if (ImGui::DragFloat3("World position", &wp.x, 0.03f))
								Owner->SetPosition(wp);
							math::Vector3 f = sp->GetWorldForward();
							if (ImGui::DragFloat3("Forward (into cone, world)", &f.x, 0.02f))
							{
								f.Normalize();
								if (f.GetSqrLength() >= 1e-10f)
								{
									sp->SetWorldForward(f);
									Owner->RotateToNewForward(-f);
									Owner->ComputeWorldTransform(0.f);
								}
							}
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
						if (ImGui::Checkbox("Cast shadow (spot depth map)", &bCastSh))
							sp->SetCastShadow(bCastSh);

						float ic = sp->GetInnerConeCos();
						float oc = sp->GetOuterConeCos();
						if (ImGui::DragFloat("Inner cone cos", &ic, 0.005f, -1.f, 1.f))
						{
							ic = (std::max)(-1.f, (std::min)(1.f, ic));
							sp->SetInnerConeCos(ic);
						}
						if (ImGui::DragFloat("Outer cone cos", &oc, 0.005f, -1.f, 1.f))
						{
							oc = (std::max)(-1.f, (std::min)(1.f, oc));
							sp->SetOuterConeCos(oc);
						}

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
					Engine::EGuiShadowFrustumKind kind{};
					math::Matrix4x4 lvp;
					math::Vector3 lightDir;
					math::Matrix4x4 cvp;
					if (srOv->TryGetGuiDebugShadowFrustum(kind, lvp, lightDir, cvp))
					{
						ImDrawList* fg = ImGui::GetForegroundDrawList();
						(void)lightDir;
						if (kind == Engine::EGuiShadowFrustumKind::DirectionalOrtho)
							DrawDirLightOrthoFrustumOverlay(fg, lvp, cvp, IM_COL32(255, 255, 255, 255));
						else
							DrawSpotShadowFrustumWireOverlay(fg, lvp, cvp, IM_COL32(255, 255, 255, 255));
					}
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
