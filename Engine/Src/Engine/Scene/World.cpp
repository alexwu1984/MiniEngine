#include "Scene/World.h"
#include "Scene/Actor.h"
#include "Scene/GltfActor.h"
#include "Scene/CameraComponent.h"
#include "Scene/SceneMeshComponent.h"
#include "Scene/SkyLightComponent.h"
#include "Scene/DirectionalLightComponent.h"
#include "Scene/PointLightComponent.h"
#include "Scene/RoamCameraActor.h"
#include "Scene/FScene.h"
#include "Engine.h"
#include "Engine/JsonConfig.h"
#include "Render/MaterialPreFrame.h"
#include "core/logger.h"
#include "core/strings.h"
#include "core/system.h"
#include <comdef.h>
#include <algorithm>
#include <cstdio>
#include <limits>
#include <string>

namespace Engine
{
	namespace
	{
		static bool ActorHasProjectingMesh(const std::shared_ptr<Actor>& actor)
		{
			if (!actor || !actor->IsActorPrivateAllocated())
				return false;
			for (const auto& comp : actor->GetAllComponents())
			{
				auto mesh = ComponentCast<SceneMeshComponent>(comp);
				if (mesh && mesh->IsProjectShadow())
					return true;
			}
			return false;
		}

		static void SpawnConfigSkyLightActor(const std::shared_ptr<World>& WorldSelf, const nlohmann::json& Evn)
		{
			if (!WorldSelf || Evn.find("Hdr") == Evn.end() || Evn["Hdr"].is_null())
				return;
			const std::string hdrUtf8 = Evn["Hdr"].get<std::string>();
			if (hdrUtf8.empty())
				return;
			std::wstring rel = core::u8_ucs2(hdrUtf8);
			if (rel.empty())
				return;
			auto actor = std::make_shared<Actor>(WorldSelf);
			actor->SetActorName(L"ConfigSkyLight");
			auto comp = std::make_shared<SkyLightComponent>(actor);
			comp->SetHDRRelativePath(std::move(rel));
			comp->SetSortPriority(0);
			try
			{
				if (Evn.find("IBLIntensity") != Evn.end() && Evn["IBLIntensity"].is_number())
					comp->SetIBLIntensity(static_cast<float>(Evn["IBLIntensity"].get<double>()));
			}
			catch (const std::exception&)
			{
			}
			actor->AddComponent(comp);
			WorldSelf->AddActor(actor);
		}

		static void SpawnConfigDirectionalLightActor(const std::shared_ptr<World>& WorldSelf, const Light& Parsed, int32_t JsonOrderIndex)
		{
			if (!WorldSelf)
				return;
			auto actor = std::make_shared<Actor>(WorldSelf);
			actor->SetActorName(std::wstring(L"ConfigDirectionalLight_") + std::to_wstring(JsonOrderIndex));
			auto dir = std::make_shared<DirectionalLightComponent>(actor);
			dir->SetUseActorForward(false);
			dir->SetWorldDirection(Parsed.Direction);
			dir->SetColor(Parsed.Color);
			dir->SetIntensity(Parsed.Intensity);
			dir->SetDepthBias(Parsed.DepthBias);
			dir->SetSortPriority(1000 - JsonOrderIndex);
			actor->AddComponent(dir);
			WorldSelf->AddActor(actor);
		}

		static std::vector<std::shared_ptr<DirectionalLightComponent>> CollectDirectionalLightComponentsSorted(
			const std::vector<std::shared_ptr<Actor>>& Actors, const std::vector<std::shared_ptr<Actor>>& PendingActors)
		{
			std::vector<std::pair<int32_t, std::shared_ptr<DirectionalLightComponent>>> pairs;
			auto append = [&pairs](const std::vector<std::shared_ptr<Actor>>& List) {
				for (const auto& a : List)
				{
					if (!a || !a->IsActorPrivateAllocated())
						continue;
					if (a->GetState() != Actor::EActive)
						continue;
					auto dir = a->GetComponent<DirectionalLightComponent>();
					if (!dir || !dir->IsEnabled())
						continue;
					pairs.emplace_back(dir->GetSortPriority(), dir);
				}
			};
			append(Actors);
			append(PendingActors);
			std::sort(pairs.begin(), pairs.end(),
					  [](const std::pair<int32_t, std::shared_ptr<DirectionalLightComponent>>& A,
						 const std::pair<int32_t, std::shared_ptr<DirectionalLightComponent>>& B) { return A.first > B.first; });
			std::vector<std::shared_ptr<DirectionalLightComponent>> out;
			out.reserve(pairs.size());
			for (auto& p : pairs)
				out.push_back(std::move(p.second));
			return out;
		}

		static std::vector<std::shared_ptr<PointLightComponent>> CollectPointLightComponentsSorted(
			const std::vector<std::shared_ptr<Actor>>& Actors, const std::vector<std::shared_ptr<Actor>>& PendingActors)
		{
			std::vector<std::pair<int32_t, std::shared_ptr<PointLightComponent>>> pairs;
			auto append = [&pairs](const std::vector<std::shared_ptr<Actor>>& List) {
				for (const auto& a : List)
				{
					if (!a || !a->IsActorPrivateAllocated())
						continue;
					if (a->GetState() != Actor::EActive)
						continue;
					auto pl = a->GetComponent<PointLightComponent>();
					if (!pl || !pl->IsEnabled())
						continue;
					pairs.emplace_back(pl->GetSortPriority(), pl);
				}
			};
			append(Actors);
			append(PendingActors);
			std::sort(pairs.begin(), pairs.end(),
					  [](const std::pair<int32_t, std::shared_ptr<PointLightComponent>>& A,
						 const std::pair<int32_t, std::shared_ptr<PointLightComponent>>& B) { return A.first > B.first; });
			std::vector<std::shared_ptr<PointLightComponent>> out;
			out.reserve(pairs.size());
			for (auto& p : pairs)
				out.push_back(std::move(p.second));
			return out;
		}

		static void SpawnConfigPointLightActor(const std::shared_ptr<World>& WorldSelf, const Light& Parsed, int32_t JsonOrderIndex,
											   const std::wstring& AttachToActorName, bool bCastShadow)
		{
			if (!WorldSelf)
				return;

			std::shared_ptr<Actor> host;
			if (!AttachToActorName.empty())
				host = WorldSelf->FindFirstActorByName(AttachToActorName);

			if (host)
			{
				auto pt = std::make_shared<PointLightComponent>(std::weak_ptr<Actor>(host));
				pt->SetColor(Parsed.Color);
				pt->SetIntensity(Parsed.Intensity);
				pt->SetRange(Parsed.Range > 0.f ? Parsed.Range : 10.f);
				pt->SetSortPriority(500 - JsonOrderIndex);
				pt->SetLocalOffset(Parsed.Position);
				pt->SetCastShadow(bCastShadow);
				host->AddComponent(pt);
				return;
			}

			auto actor = std::make_shared<Actor>(WorldSelf);
			actor->SetActorName(std::wstring(L"ConfigPointLight_") + std::to_wstring(JsonOrderIndex));
			actor->SetPosition(Parsed.Position);
			auto pt = std::make_shared<PointLightComponent>(actor);
			pt->SetColor(Parsed.Color);
			pt->SetIntensity(Parsed.Intensity);
			pt->SetRange(Parsed.Range > 0.f ? Parsed.Range : 10.f);
			pt->SetSortPriority(500 - JsonOrderIndex);
			pt->SetCastShadow(bCastShadow);
			actor->AddComponent(pt);
			WorldSelf->AddActor(actor);
		}

		/** Evn.Light[] KHR-style strings; used only at scene load to spawn light actors. */
		/** Evn.RotateIBL: "pitchDeg,yawDeg" — authoritative on primary SkyLightComponent (UE-style). */
		static void ApplyEvnRotateIBL(const std::shared_ptr<World>& world, const nlohmann::json& evnJson)
		{
			try
			{
				if (!world || evnJson.find("RotateIBL") == evnJson.end() || !evnJson["RotateIBL"].is_string())
					return;
				const std::string s = evnJson["RotateIBL"].get<std::string>();
				float x = 0.f, y = 0.f;
				if (std::sscanf(s.c_str(), "%f,%f", &x, &y) < 2)
					return;
				const auto sl = world->FindPrimarySkyLightComponent();
				if (!sl)
					return;
				sl->SetIBLRotationPitchDegrees(x);
				sl->SetIBLRotationYawDegrees(y);
			}
			catch (const std::exception&)
			{
			}
		}

		static bool ParseEvnLightJsonEntry(const nlohmann::json& lightInfoJson, Light& lightInfo)
		{
			try
			{
				lightInfo = Light{};
				const std::string colorStr = lightInfoJson.at("LightColor").get<std::string>();
				if (std::sscanf(colorStr.c_str(), "%f,%f,%f", &lightInfo.Color.x, &lightInfo.Color.y, &lightInfo.Color.z) < 3)
					return false;
				lightInfo.Type = lightInfoJson.at("LightType").get<int>();
				lightInfo.Intensity = static_cast<float>(lightInfoJson.at("LightStrength").get<double>());
				if (lightInfo.Type == LightType_Point)
				{
					lightInfo.Position = math::Vector3(0.f, 0.f, 0.f);
					if (const auto pit = lightInfoJson.find("LightPosition"); pit != lightInfoJson.end() && pit->is_string())
					{
						const std::string posStr = pit->get<std::string>();
						std::sscanf(posStr.c_str(), "%f,%f,%f", &lightInfo.Position.x, &lightInfo.Position.y, &lightInfo.Position.z);
					}
					lightInfo.Range = 10.f;
					if (const auto rit = lightInfoJson.find("LightRange"); rit != lightInfoJson.end() && rit->is_number())
						lightInfo.Range = static_cast<float>(rit->get<double>());
					lightInfo.Direction = math::Vector3(0.f, -1.f, 0.f);
					return true;
				}

				const std::string dirStr = lightInfoJson.at("LightDir").get<std::string>();
				if (std::sscanf(dirStr.c_str(), "%f,%f,%f", &lightInfo.Direction.x, &lightInfo.Direction.y, &lightInfo.Direction.z) < 3)
					return false;
				return true;
			}
			catch (const std::exception&)
			{
				return false;
			}
		}

		static std::shared_ptr<SkyLightComponent> FindBestSkyLightInList(const std::vector<std::shared_ptr<Actor>>& Actors)
		{
			std::shared_ptr<SkyLightComponent> best;
			int32_t bestPri = (std::numeric_limits<int32_t>::min)();
			for (const auto& a : Actors)
			{
				if (!a || !a->IsActorPrivateAllocated())
					continue;
				if (a->GetState() != Actor::EActive)
					continue;
				auto sl = a->GetComponent<SkyLightComponent>();
				if (!sl || !sl->IsEnabled())
					continue;
				if (sl->GetHDRRelativePath().empty())
					continue;
				const int32_t p = sl->GetSortPriority();
				if (!best || p > bestPri)
				{
					best = sl;
					bestPri = p;
				}
			}
			return best;
		}
	}

	struct WorldPrivate
	{
		/** First field: destroyed last — primitives unregister from scene before FScene is released. */
		std::shared_ptr<FScene> Scene;
		std::vector<std::shared_ptr<Actor>> Actors;
		std::vector<std::shared_ptr<Actor>> PendingActors;
		std::shared_ptr<CameraComponent> MainCamera;
		bool UpdatingActors = false;
		mutable std::recursive_mutex lock;

		mutable bool ShadowProjectorCacheDirty = true;
		mutable std::weak_ptr<Actor> ShadowProjectorCache;
		bool bLoadedSceneUsesRoamCamera = false;
	};

	World::World()
		: d_ptr(new WorldPrivate())
	{
		C_P(World);
		d->Scene = std::make_shared<FScene>();
	}

	World::~World()
	{
		delete d_ptr;
		d_ptr = nullptr;
	}

	std::shared_ptr<Actor> World::FindFirstActorByName(const std::wstring& Name) const
	{
		if (Name.empty())
			return nullptr;
		C_P(const World);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		auto scan = [&](const std::vector<std::shared_ptr<Actor>>& Vec) -> std::shared_ptr<Actor> {
			for (const auto& a : Vec)
				if (a && a->IsActorPrivateAllocated() && a->GetActorName() == Name)
					return a;
			return nullptr;
		};
		if (auto f = scan(d->Actors))
			return f;
		return scan(d->PendingActors);
	}

	void World::LoadScene(const std::wstring& ModelFile)
	{
		C_P(World);
		nlohmann::json Root;
		if (!LoadJsonFile(ModelFile, Root))
			return;
		Engine::GEngine->LoadConfig(ModelFile, Root);

		try
		{
			const auto self = this->shared_from_this();

			nlohmann::json Models = Root["Modles"];
			bool sceneHasRoam = false;
			for (const auto& Model : Models)
			{
				if (Model.find("RoamCamera") != Model.end() && Model["RoamCamera"].is_object())
					sceneHasRoam = true;
			}
			d->bLoadedSceneUsesRoamCamera = sceneHasRoam;

			for (const auto& Model : Models)
			{
				if (Model.find("RoamCamera") != Model.end() && Model["RoamCamera"].is_object())
				{
					auto roam = std::make_shared<RoamCameraActor>(self, Model["RoamCamera"]);
					roam->InitResouce();
					AddActor(roam);
					continue;
				}
				auto AGltfModel = std::make_shared<Engine::GltfActor>(self, Model);
				AGltfModel->InitResouce();
				// Load failure: InitResouce returns without AddComponent(mesh). Skip AddActor so RoamCamera+Evn still work.
				if (AGltfModel->GetComponent<SceneMeshComponent>())
					AddActor(AGltfModel);
				else
				{
					core::LOG(core::log_e::log_err,
							  L"World::LoadScene: GltfActor has no SceneMeshComponent after InitResouce (model load failed). Actor=%s",
							  AGltfModel->GetActorName().c_str());
				}
			}

			nlohmann::json evnJson = Root["Evn"];
			SpawnConfigSkyLightActor(self, evnJson);
			ApplyEvnRotateIBL(self, evnJson);

			const nlohmann::json lightJsons = evnJson["Light"];
			int32_t directionalJsonOrder = 0;
			int32_t pointJsonOrder = 0;
			for (const auto& lightInfoJson : lightJsons)
			{
				Light lightInfo{};
				if (!ParseEvnLightJsonEntry(lightInfoJson, lightInfo))
					continue;
				if (lightInfo.Type == LightType_Directional)
				{
					SpawnConfigDirectionalLightActor(self, lightInfo, directionalJsonOrder);
					++directionalJsonOrder;
					continue;
				}
				if (lightInfo.Type == LightType_Point)
				{
					std::wstring attachName;
					if (const auto ait = lightInfoJson.find("AttachActor"); ait != lightInfoJson.end() && ait->is_string())
						attachName = core::u8_ucs2(ait->get<std::string>());
					bool castShadow = false;
					if (const auto cs = lightInfoJson.find("CastShadow"); cs != lightInfoJson.end() && cs->is_boolean())
						castShadow = cs->get<bool>();
					SpawnConfigPointLightActor(self, lightInfo, pointJsonOrder, attachName, castShadow);
					++pointJsonOrder;
					continue;
				}
			}
		}
		catch (const _com_error& e)
		{
			const wchar_t* msg = e.ErrorMessage();
			if (!msg)
				msg = L"(null ErrorMessage)";
			core::LOG(core::log_e::log_err,
					  L"World::LoadScene: _com_error HRESULT=0x%08X %s",
					  (unsigned)e.Error(),
					  msg);
		}
		catch (const std::exception& e)
		{
			core::LOG(core::log_e::log_err,
					  L"World::LoadScene: %s",
					  core::u8_ucs2(e.what()).c_str());
		}
		catch (...)
		{
			core::LOG(core::log_e::log_err, L"World::LoadScene: unknown C++ exception");
		}
	}

	void World::AddActor(std::shared_ptr<Actor> actor)
	{
		C_P(World);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		if (d->UpdatingActors)
		{
			d->PendingActors.emplace_back(actor);
		}
		else
		{
			d->Actors.emplace_back(actor);
		}
		RefreshShadowProjectorForActor(actor);
	}

	void World::RemoveActor(std::shared_ptr<Actor> actor)
	{
		C_P(World);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		auto iter = std::find(d->PendingActors.begin(), d->PendingActors.end(), actor);
		if (iter != d->PendingActors.end())
		{
			std::iter_swap(iter, d->PendingActors.end() - 1);
			d->PendingActors.pop_back();
		}

		iter = std::find(d->Actors.begin(), d->Actors.end(), actor);
		if (iter != d->Actors.end())
		{
			std::iter_swap(iter, d->Actors.end() - 1);
			d->Actors.pop_back();
		}
		RefreshShadowProjectorForActor(nullptr);
	}

	void World::RemoveAllActors()
	{
		C_P(World);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		d->Actors.clear();
		d->PendingActors.clear();
		d->MainCamera.reset();
		RefreshShadowProjectorForActor(nullptr);
	}

	void World::DispatchInput(const InputDeviceState& InputState)
	{
		C_P(World);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		for (auto Item : d->Actors)
		{
			if (Item->GetState() == Actor::EActive)
			{
				Item->ProcessInput(InputState);
			}
		}
	}

	void World::TickSimulation(float DeltaTime)
	{
		C_P(World);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		d->UpdatingActors = true;
		for (auto Item : d->Actors)
		{
			if (Item->GetState() == Actor::EActive)
			{
				Item->Tick(DeltaTime);
			}
		}

		d->UpdatingActors = false;

		for (auto pending : d->PendingActors)
		{
			pending->Tick(DeltaTime);
			d->Actors.emplace_back(pending);
		}

		d->PendingActors.clear();
		RefreshShadowProjectorForActor(nullptr);
	}

	void World::SetMainCamera(std::shared_ptr<CameraComponent> Camera)
	{
		C_P(World);
		d->MainCamera = Camera;
	}

	void World::InvalidatePrimaryViewStateAfterSceneCut()
	{
		// Camera-side temporal invalidation (generation, prev/jitter/frame counters). UE: FSceneViewState / view history invalidation.
		// Pairs with FWorldSceneRender::NotifyWorldRenderingSceneChanged (renderer transients + PP temporal).
		// - TemporalHistoryGeneration → FSceneViewData → TemporallAA / SSR bootstrap.
		// - bTemporalPrevMatricesValid=false → next Tick's EnsureTemporalPrevMatricesInitialized (ViewportClient tick before Render).
		if (const auto cam = GetMainCamera())
			cam->MarkTemporalHistoryStaleAfterSceneCut();
	}

	std::shared_ptr<Engine::CameraComponent> World::GetMainCamera() const
	{
		C_P(const World);
		return d->MainCamera;
	}

	const std::vector<std::shared_ptr<Actor>>& World::GetAllActors() const
	{
		C_P(const World);
		return d->Actors;
	}

	std::vector<std::shared_ptr<Actor>> World::GetAllActorsCopy() const
	{
		C_P(const World);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		return d->Actors;
	}

	std::shared_ptr<FScene> World::GetScene() const
	{
		C_P(const World);
		return d->Scene;
	}

	std::shared_ptr<DirectionalLightComponent> World::GetPrimaryDirectionalLightForEditing() const
	{
		C_P(const World);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		const auto sorted = CollectDirectionalLightComponentsSorted(d->Actors, d->PendingActors);
		return sorted.empty() ? nullptr : sorted.front();
	}

	std::vector<std::shared_ptr<DirectionalLightComponent>> World::GetDirectionalLightsForEditingSorted() const
	{
		C_P(const World);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		return CollectDirectionalLightComponentsSorted(d->Actors, d->PendingActors);
	}

	std::vector<std::shared_ptr<PointLightComponent>> World::GetPointLightsForEditingSorted() const
	{
		C_P(const World);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		return CollectPointLightComponentsSorted(d->Actors, d->PendingActors);
	}

	std::vector<Light> World::GatherLightsForView() const
	{
		C_P(const World);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		const auto dirComps = CollectDirectionalLightComponentsSorted(d->Actors, d->PendingActors);
		const auto pointComps = CollectPointLightComponentsSorted(d->Actors, d->PendingActors);
		std::vector<Light> out;
		out.reserve((std::min)(static_cast<size_t>(MAX_LIGHT_INSTANCES), dirComps.size() + pointComps.size()));
		for (const auto& comp : dirComps)
		{
			if (out.size() >= MAX_LIGHT_INSTANCES)
				break;
			out.push_back(comp->BuildLight());
		}
		for (const auto& comp : pointComps)
		{
			if (out.size() >= MAX_LIGHT_INSTANCES)
				break;
			out.push_back(comp->BuildLight());
		}
		return out;
	}

	void World::RefreshShadowProjectorForActor(std::shared_ptr<Actor> actor)
	{
		(void)actor;
		C_P(World);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		d->ShadowProjectorCacheDirty = true;
	}

	std::shared_ptr<Actor> World::GetShadowProjectorActor() const
	{
		C_P(const World);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		if (!d->ShadowProjectorCacheDirty)
		{
			if (auto cached = d->ShadowProjectorCache.lock())
				return cached;
		}
		d->ShadowProjectorCacheDirty = false;
		std::shared_ptr<Actor> found;
		for (const auto& a : d->Actors)
		{
			if (ActorHasProjectingMesh(a))
			{
				found = a;
				break;
			}
		}
		if (!found)
		{
			for (const auto& a : d->PendingActors)
			{
				if (ActorHasProjectingMesh(a))
				{
					found = a;
					break;
				}
			}
		}
		d->ShadowProjectorCache = found;
		return found;
	}

	FShadowProjectorSceneData World::BuildShadowProjectorAggregateData() const
	{
		C_P(const World);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		FShadowProjectorSceneData out{};
		math::AABB3 merged{};
		bool any = false;
		auto scan = [&](const std::vector<std::shared_ptr<Actor>>& list) {
			for (const auto& a : list)
			{
				if (!a || !a->IsActorPrivateAllocated() || a->GetState() != Actor::EActive)
					continue;
				for (const auto& c : a->GetAllComponents())
				{
					auto mesh = ComponentCast<SceneMeshComponent>(c);
					if (!mesh || !mesh->IsProjectShadow())
						continue;
					const math::AABB3 wbox = mesh->GetShadowFrustumWorldBounds();
					merged = any ? merged.MergeAABB(wbox) : wbox;
					any = true;
				}
			}
		};
		scan(d->Actors);
		scan(d->PendingActors);
		if (!any)
			return out;
		out.bValid = true;
		out.WorldTransform = math::Matrix4x4::ms_Materix3X3WIdentity;
		out.ModelLocalAABB = merged;
		return out;
	}

	std::shared_ptr<SkyLightComponent> World::FindPrimarySkyLightComponent() const
	{
		C_P(const World);
		std::lock_guard<std::recursive_mutex> l(d->lock);
		auto fromActors = FindBestSkyLightInList(d->Actors);
		auto fromPending = FindBestSkyLightInList(d->PendingActors);
		if (!fromActors)
			return fromPending;
		if (!fromPending)
			return fromActors;
		if (fromActors->GetSortPriority() >= fromPending->GetSortPriority())
			return fromActors;
		return fromPending;
	}

	std::optional<std::wstring> World::ResolvePrimarySkyLightHDRFullPath() const
	{
		const auto sl = FindPrimarySkyLightComponent();
		if (!sl)
			return std::nullopt;
		return sl->ResolveHDRFullPath();
	}

	float World::GetSkyLightIBLScale() const
	{
		const auto sl = FindPrimarySkyLightComponent();
		if (!sl || !sl->IsEnabled() || sl->GetHDRRelativePath().empty())
			return 0.f;
		const float i = sl->GetIBLIntensity();
		return i > 0.f ? i : 0.f;
	}

	void World::GetPrimarySkyLightIBLRotationDegrees(float& outPitchDeg, float& outYawDeg) const
	{
		outPitchDeg = 0.f;
		outYawDeg = 0.f;
		const auto sl = FindPrimarySkyLightComponent();
		if (!sl)
			return;
		outPitchDeg = sl->GetIBLRotationPitchDegrees();
		outYawDeg = sl->GetIBLRotationYawDegrees();
	}

	bool World::UsesRoamCameraScene() const
	{
		C_P(const World);
		return d->bLoadedSceneUsesRoamCamera;
	}
}
