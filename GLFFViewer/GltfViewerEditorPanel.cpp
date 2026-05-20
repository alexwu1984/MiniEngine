#include "core/inc.h"
#include "GltfViewerEditorPanel.h"
#include "Engine/Engine.h"
#include "Engine/Scene/Actor.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/WorldSceneDebugDraw.h"
#include "Engine/Scene/WorldDirectionalShadowCSMSettings.h"
#include "Scene/SkyLightComponent.h"
#include "Engine/Scene/DirectionalLightComponent.h"
#include "Scene/PointLightComponent.h"
#include "Scene/SpotLightComponent.h"
#include "Scene/SceneMeshComponent.h"
#include "Render/WorldSceneRender.h"
#include "Render/GBufferVisualization.h"
#include "Render/Shadow/FDirectionalShadowFrustumFitter.h"
#include "Render/RDGBuilder.h"
#include "RHI/DynamicRHI.h"
#include "Imgui/imgui.h"
#include "core/strings.h"
#include "math/vector3.h"

using namespace Engine;

namespace
{
	bool ActorHostsSceneMesh(const std::shared_ptr<Actor>& Owner)
	{
		if (!Owner)
			return false;
		for (const auto& c : Owner->GetAllComponents())
			if (ComponentCast<SceneMeshComponent>(c))
				return true;
		return false;
	}

	std::string ActorNameOrFallbackUtf8(const std::shared_ptr<Actor>& Owner, const char* FallbackPrefix, int Idx)
	{
		if (!Owner)
			return std::string(FallbackPrefix) + " #" + std::to_string(Idx);
		const std::wstring& n = Owner->GetActorName();
		if (!n.empty())
			return core::ucs2_u8(n);
		return std::string(FallbackPrefix) + " #" + std::to_string(Idx);
	}

	void ConeAxisToYawPitchDeg(const math::Vector3& coneIn, float& yawDeg, float& pitchDeg)
	{
		math::Vector3 c = coneIn;
		if (c.GetSqrLength() < 1e-10f)
			c = math::Vector3(0.f, 0.f, 1.f);
		else
			c = c.Normalize();
		pitchDeg = math::Asin(math::Clamp(c.y, -1.f, 1.f)) * (180.f / math::MATH_PI);
		yawDeg = math::Atan2(c.x, c.z) * (180.f / math::MATH_PI);
	}

	math::Vector3 YawPitchDegToConeAxis(float yawDeg, float pitchDeg)
	{
		const float yr = yawDeg * (math::MATH_PI / 180.f);
		const float pr = pitchDeg * (math::MATH_PI / 180.f);
		const float cy = math::Cos(yr), sy = math::Sin(yr);
		const float cp = math::Cos(pr), sp = math::Sin(pr);
		math::Vector3 v(sy * cp, sp, cy * cp);
		if (v.GetSqrLength() < 1e-10f)
			return math::Vector3(0.f, 0.f, 1.f);
		return v.Normalize();
	}

	float SpotCosToHalfAngleDeg(float cosHalfAngle)
	{
		const float c = math::Clamp(cosHalfAngle, -1.f, 1.f);
		return math::Acos(c) * (180.f / math::MATH_PI);
	}

	float SpotHalfAngleDegToCos(float halfAngleDeg)
	{
		halfAngleDeg = math::Clamp(halfAngleDeg, 0.f, 89.f);
		const float rad = halfAngleDeg * (math::MATH_PI / 180.f);
		return math::Cos(rad);
	}
}

void GltfViewerEditorPanel::SetModelSelection(std::vector<std::string> InModelLabelsUtf8, int32_t* InSelectedIndex,
											  std::atomic<int32_t>* InPendingModelIndex)
{
	ModelLabelsUtf8 = std::move(InModelLabelsUtf8);
	SelectedIndex = InSelectedIndex;
	PendingModelIndex = InPendingModelIndex;
}

void GltfViewerEditorPanel::Bind(FWorldSceneRender& SceneRender)
{
	Unbind(SceneRender);
	SceneRender.sigGuiEvent.bind([this] { Draw(); }, this);
	bBound = true;
}

void GltfViewerEditorPanel::Unbind(FWorldSceneRender& SceneRender)
{
	if (!bBound)
		return;
	SceneRender.sigGuiEvent.unbind(this);
	bBound = false;
}

void GltfViewerEditorPanel::Draw()
{
	auto Scene = GEngine ? GEngine->GetWorld() : nullptr;
	if (!Scene)
		return;

	ImGui::SetNextWindowPos(ImVec2(1, 1));
	ImGui::SetNextWindowSize(ImVec2(420, 520), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("GLTF Viewer", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	DrawModelCombo(*Scene);
	DrawViewportSection(*Scene);
	DrawDirectionalCsmSection(*Scene);
	DrawSceneLightsSection(*Scene);

	ImGui::Separator();
	ImGui::TextUnformatted("SkyLight / IBL");
	ImGui::TextDisabled("HDR rotation removed (default orbit camera).");

	DrawModelTransformSection(*Scene);
	DrawGBufferSection();
	DrawPerformanceSection();

	ImGui::End();
}

void GltfViewerEditorPanel::DrawModelCombo(World& Scene)
{
	(void)Scene;
	if (ModelLabelsUtf8.empty() || !SelectedIndex || !PendingModelIndex)
		return;

	const char* preview = (*SelectedIndex >= 0 && *SelectedIndex < static_cast<int>(ModelLabelsUtf8.size()))
		? ModelLabelsUtf8[static_cast<size_t>(*SelectedIndex)].c_str()
		: "";
	if (!ImGui::BeginCombo("Model", preview))
		return;

	for (int i = 0; i < static_cast<int>(ModelLabelsUtf8.size()); ++i)
	{
		const bool isSelected = (i == *SelectedIndex);
		if (ImGui::Selectable(ModelLabelsUtf8[static_cast<size_t>(i)].c_str(), isSelected))
			PendingModelIndex->store(i, std::memory_order_release);
		if (isSelected)
			ImGui::SetItemDefaultFocus();
	}
	ImGui::EndCombo();
}

void GltfViewerEditorPanel::DrawViewportSection(World& Scene)
{
	ImGui::Separator();
	ImGui::TextUnformatted("Viewport");

	bool bShowBounds = Scene.GetSceneDebugDraw().GetShowSceneMeshBoundsDebug();
	if (ImGui::Checkbox("Show model bounds (UE-style)", &bShowBounds))
		Scene.GetSceneDebugDraw().SetShowSceneMeshBoundsDebug(bShowBounds);

	bool bShowCasterBounds = Scene.GetSceneDebugDraw().GetShowShadowCasterMeshBoundsDebug();
	if (ImGui::Checkbox("Show shadow caster bounds", &bShowCasterBounds))
		Scene.GetSceneDebugDraw().SetShowShadowCasterMeshBoundsDebug(bShowCasterBounds);
}

void GltfViewerEditorPanel::DrawDirectionalCsmSection(World& Scene)
{
	if (!Scene.GetDirectionalShadowCSMSettings().GetShowUi())
		return;

	ImGui::Separator();
	ImGui::TextUnformatted("Directional CSM (scene)");
	ImGui::TextWrapped("Split0/1 are linear in [Near,Far] on ze=dot(world-cam,viewForward). With Far=1000, "
					   "use small values (e.g. 0.002~0.02) so cuts fall near the bike; large defaults put the first cut at tens of meters.");

	bool csmEn = Scene.GetDirectionalShadowCSMSettings().GetEnabled();
	if (ImGui::Checkbox("Enable cascades", &csmEn))
		Scene.GetDirectionalShadowCSMSettings().SetEnabled(csmEn);

	int casc = static_cast<int>(Scene.GetDirectionalShadowCSMSettings().GetCascadeCount());
	if (ImGui::SliderInt("Cascade count", &casc, 2, 3))
		Scene.GetDirectionalShadowCSMSettings().SetCascadeCount(static_cast<int32_t>(casc));

	float s0 = Scene.GetDirectionalShadowCSMSettings().GetSplit0();
	const float smin = FDirectionalShadowFrustumFitter::kCascadeSplitNormMin;
	const float smax = FDirectionalShadowFrustumFitter::kCascadeSplitNormMax;
	if (ImGui::SliderFloat("Split0 (linear along near..far)", &s0, smin, smax))
		Scene.GetDirectionalShadowCSMSettings().SetSplit0(s0);

	float s1 = Scene.GetDirectionalShadowCSMSettings().GetSplit1();
	if (ImGui::SliderFloat("Split1 (3 cascades only)", &s1, smin, smax))
		Scene.GetDirectionalShadowCSMSettings().SetSplit1(s1);

	bool bCasBounds = Scene.GetSceneDebugDraw().GetShowDirectionalCSMCascadeSubjectBoundsDebug();
	if (ImGui::Checkbox("Draw CSM cascade subject AABBs (green/cyan/magenta)", &bCasBounds))
		Scene.GetSceneDebugDraw().SetShowDirectionalCSMCascadeSubjectBoundsDebug(bCasBounds);
}

void GltfViewerEditorPanel::DrawSceneLightsSection(World& Scene)
{
	ImGui::Separator();
	ImGui::TextUnformatted("Scene light components");

	const std::vector<std::shared_ptr<DirectionalLightComponent>> dirs = Scene.GetDirectionalLightsForEditingSorted();
	const std::vector<std::shared_ptr<PointLightComponent>> pts = Scene.GetPointLightsForEditingSorted();
	const std::vector<std::shared_ptr<SpotLightComponent>> spts = Scene.GetSpotLightsForEditingSorted();
	if (dirs.empty() && pts.empty() && spts.empty())
		ImGui::TextDisabled("(No DirectionalLight / PointLight / SpotLight components)");

	for (int i = 0; i < static_cast<int>(dirs.size()); ++i)
	{
		const auto& dir = dirs[static_cast<size_t>(i)];
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
				if (auto sl = Scene.FindPrimarySkyLightComponent(); sl && sl->IsEnabled() && sl->IsProceduralSky())
					sl->SetProceduralSunDirectionTowardSource(nd);
			}

			math::Vector3 col = dir->GetColor();
			if (ImGui::DragFloat3("Color RGB (linear)", &col.x, 0.03f, 0.f, 32.f))
				dir->SetColor(col);

			float stren = dir->GetIntensity();
			if (ImGui::DragFloat("Intensity", &stren, 0.1f, 0.f, 50.f))
				dir->SetIntensity(math::Max(stren, 0.f));

			bool bShowSh = dir->GetShowShadowFrustumDebug();
			if (ImGui::Checkbox("Show shadow frustum", &bShowSh))
				dir->SetShowShadowFrustumDebug(bShowSh);
			ImGui::TextDisabled("Draws ortho shadow frustum when this lamp owns directional shadow.");

			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	ImGui::Separator();

	for (int i = 0; i < static_cast<int>(pts.size()); ++i)
	{
		const auto& pl = pts[static_cast<size_t>(i)];
		const auto Owner = pl ? pl->GetOwner() : nullptr;
		ImGui::PushID(200000 + i);
		const std::string titlePt = std::string("Point: ") + ActorNameOrFallbackUtf8(Owner, "PointLight", i);
		if (ImGui::TreeNodeEx(titlePt.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ActorHostsSceneMesh(Owner))
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
				pl->SetIntensity(math::Max(strenp, 0.f));

			float rng = pl->GetRange();
			if (ImGui::DragFloat("Attenuation range", &rng, 0.25f, 0.05f, 256.f))
				pl->SetRange(math::Max(rng, 0.01f));

			bool bCastPt = pl->GetCastShadow();
			if (ImGui::Checkbox("Cast shadow", &bCastPt))
				pl->SetCastShadow(bCastPt);

			bool bShowSh = pl->GetShowShadowFrustumDebug();
			if (ImGui::Checkbox("Show shadow bounds", &bShowSh))
				pl->SetShowShadowFrustumDebug(bShowSh);
			ImGui::TextDisabled("Draws cube shadow bounds when this lamp owns cubemap shadow.");

			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	for (int i = 0; i < static_cast<int>(spts.size()); ++i)
	{
		const auto& sp = spts[static_cast<size_t>(i)];
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
				sp->SetIntensity(math::Max(st, 0.f));

			float rngs = sp->GetRange();
			if (ImGui::DragFloat("Range (<0 unlimited)", &rngs, 0.5f, -1.f, 2000.f))
				sp->SetRange(rngs);

			bool bCastSh = sp->GetCastShadow();
			if (ImGui::Checkbox("Cast shadow", &bCastSh))
				sp->SetCastShadow(bCastSh);

			bool bShowSf = sp->GetShowShadowFrustumDebug();
			if (ImGui::Checkbox("Show shadow frustum", &bShowSf))
				sp->SetShowShadowFrustumDebug(bShowSf);
			ImGui::TextDisabled("Draws pyramid when this lamp owns spot shadow map.");

			float innerDeg = SpotCosToHalfAngleDeg(sp->GetInnerConeCos());
			float outerDeg = SpotCosToHalfAngleDeg(sp->GetOuterConeCos());
			const bool innerCh = ImGui::DragFloat("Inner half-angle (deg)", &innerDeg, 0.5f, 0.f, 89.f);
			const bool outerCh = ImGui::DragFloat("Outer half-angle (deg)", &outerDeg, 0.5f, 1.f, 89.f);
			if (innerCh || outerCh)
			{
				outerDeg = math::Clamp(outerDeg, 1.f, 89.f);
				innerDeg = math::Clamp(innerDeg, 0.f, 89.f);
				if (innerDeg > outerDeg)
					innerDeg = outerDeg;
				sp->SetOuterConeCos(SpotHalfAngleDegToCos(outerDeg));
				sp->SetInnerConeCos(SpotHalfAngleDegToCos(innerDeg));
			}

			ImGui::TreePop();
		}
		ImGui::PopID();
	}
}

void GltfViewerEditorPanel::DrawModelTransformSection(World& Scene)
{
	ImGui::Separator();
	ImGui::TextUnformatted("Scene model transform");

	const auto actors = Scene.GetAllActors();
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

void GltfViewerEditorPanel::DrawGBufferSection()
{
	if (!ImGui::CollapsingHeader("GBuffer", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	auto srVis = GEngine ? GEngine->GetSceneRender() : nullptr;
	if (!srVis)
	{
		ImGui::TextDisabled("(Scene render not ready)");
		return;
	}

	FGBufferVisualizationSettings& vis = srVis->GetGBufferVisualizationSettings();
	static const char* kGBufferModeLabels[] = {
		"Off",
		"Base Color (pre-lighting)",
		"World Normal",
		"Metallic",
		"Roughness",
		"Ambient Occlusion",
		"Emissive",
		"Depth (linear view Z)",
		"Shading Model ID",
		"Lit Scene Color",
	};
	int mode = static_cast<int>(vis.Mode);
	const int modeCount = static_cast<int>(sizeof(kGBufferModeLabels) / sizeof(kGBufferModeLabels[0]));
	if (ImGui::Combo("Buffer", &mode, kGBufferModeLabels, modeCount))
		vis.Mode = static_cast<EGBufferVisualizeMode>(mode);
	ImGui::TextDisabled("Replaces viewport SceneColor before post-process (RDG pass GBufferVisualization).");
}

void GltfViewerEditorPanel::DrawPerformanceSection()
{
	if (!ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	const ImGuiIO& io = ImGui::GetIO();
	ImGui::Text("%.1f FPS   %.3f ms/frame", io.Framerate, static_cast<double>(io.DeltaTime * 1000.0f));
	ImGui::Separator();

	if (GEngine)
	{
		const char* apiLabel = GEngine->GetInitRHIApiType() == RenderCore::RHIAPIType::E_D3D11 ? "D3D11" : "D3D12";
		ImGui::TextUnformatted(apiLabel);
	}
	ImGui::TextDisabled("CPU: render-thread wall clock in Execute(). GPU: prior-frame segment (timestamp queries).");

	auto srPipe = GEngine ? GEngine->GetSceneRender() : nullptr;
	if (!srPipe)
		return;

	std::vector<FRDGPassCpuTiming> rows;
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
