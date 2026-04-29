#pragma once
#include "core/inc.h"
#include "Scene/DeviceInputState.h"
#include "Render/MaterialPreFrame.h"
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace Engine
{
	class Actor;
	class CameraComponent;
	struct WorldPrivate;

	/** Game-thread world: actors, lights, main camera (UE world / scene subset; no input, no tick). */
	class World : public std::enable_shared_from_this<World>
	{
	public:
		World();
		~World();

		void LoadScene(const std::wstring& ModelFile);
		void AddActor(std::shared_ptr<Actor> actor);
		void RemoveActor(std::shared_ptr<Actor> actor);
		void RemoveAllActors();
		void TickSimulation(float DeltaTime);
		void DispatchInput(const InputDeviceState& InputState);

		template<typename ActorType> std::vector<std::shared_ptr<ActorType>> GetActors()
		{
			std::vector<std::shared_ptr<ActorType>> Actors;
			for (auto ActorItem : GetAllActors())
			{
				std::shared_ptr<ActorType> ConvertActor = ActorCast<ActorType>(ActorItem);
				if (ConvertActor)
				{
					Actors.push_back(ConvertActor);
				}
			}
			return Actors;
		}
		void SetMainCamera(std::shared_ptr<CameraComponent> Camera);
		std::shared_ptr<CameraComponent> GetMainCamera() const;
		const std::vector<std::shared_ptr<Actor>>& GetAllActors() const;
		const std::vector<Light>& GetLights() const;
		std::vector<Light>& GetLights();

		/** Call when an actor's mesh components or shadow flags change; invalidates shadow-projector cache. */
		void RefreshShadowProjectorForActor(std::shared_ptr<Actor> actor);
		/** First actor that owns a GltfMeshComponent with project-shadow enabled (same order as scene iteration). */
		std::shared_ptr<Actor> GetShadowProjectorActor() const;

	private:
		WorldPrivate* d_ptr = nullptr;
	};
}
