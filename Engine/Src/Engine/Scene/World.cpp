#include "Scene/World.h"
#include "Scene/Actor.h"
#include "Scene/GltfActor.h"
#include "Scene/CameraComponent.h"
#include "Scene/GltfMeshComponent.h"
#include "Engine.h"
#include "Engine/JsonConfig.h"

namespace Engine
{
	namespace
	{
		static bool ActorHasProjectingMesh(const std::shared_ptr<Actor>& actor)
		{
			if (!actor)
				return false;
			for (const auto& comp : actor->GetAllComponents())
			{
				auto mesh = ComponentCast<GltfMeshComponent>(comp);
				if (mesh && mesh->IsProjectShadow())
					return true;
			}
			return false;
		}
	}

	struct WorldPrivate
	{
		std::vector<std::shared_ptr<Actor>> Actors;
		std::vector<std::shared_ptr<Actor>> PendingActors;
		std::shared_ptr<CameraComponent> MainCamera;
		bool UpdatingActors = false;
		mutable std::recursive_mutex lock;
		std::vector<Light> lightInfos;

		mutable bool ShadowProjectorCacheDirty = true;
		mutable std::weak_ptr<Actor> ShadowProjectorCache;
	};

	World::World()
		: d_ptr(new WorldPrivate())
	{
	}

	World::~World()
	{
		delete d_ptr;
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
			nlohmann::json Models = Root["Modles"];
			for (const auto& Model : Models)
			{
				auto AGltfModel = std::make_shared<Engine::GltfActor>(this->shared_from_this(), Model);
				AGltfModel->InitResouce();
				AddActor(AGltfModel);
			}

			nlohmann::json evnJson = Root["Evn"];
			const nlohmann::json lightJsons = evnJson["Light"];
			for (const auto& lightInfoJson : lightJsons)
			{
				Light lightInfo;
				std::string colorStr = lightInfoJson["LightColor"];
				std::sscanf(colorStr.c_str(), "%f,%f,%f", &lightInfo.Color.x, &lightInfo.Color.y, &lightInfo.Color.z);
				std::string dirStr = lightInfoJson["LightDir"];
				std::sscanf(dirStr.c_str(), "%f,%f,%f", &lightInfo.Direction.x, &lightInfo.Direction.y, &lightInfo.Direction.z);
				lightInfo.Type = lightInfoJson["LightType"];
				lightInfo.Intensity = lightInfoJson["LightStrength"];
				d->lightInfos.emplace_back(lightInfo);
			}
		}
		catch (const std::exception& e)
		{
			std::string error = e.what();
			(void)error;
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
		for (auto ItActor = d->Actors.begin(); ItActor != d->Actors.end(); ++ItActor)
		{
			(*ItActor)->SetState(Actor::EDead);
		}
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

	const std::vector<Light>& World::GetLights() const
	{
		C_P(const World);
		return d->lightInfos;
	}

	std::vector<Light>& World::GetLights()
	{
		C_P(World);
		return d->lightInfos;
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
}
