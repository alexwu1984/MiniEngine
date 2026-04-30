#pragma once
#include "core/inc.h"
#include "Scene/DeviceInputState.h"
#include "Render/MaterialPreFrame.h"
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

namespace Engine
{
	class Actor;
	class CameraComponent;
	class SkyLightComponent;
	class DirectionalLightComponent;
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
		/** Thread-safe copy of current actors (holds World's lock); prefer this when copying off the mutation path. */
		std::vector<std::shared_ptr<Actor>> GetAllActorsCopy() const;
		/** Same ordering as the first entry in GatherLightsForView (highest SortPriority among enabled directionals). */
		std::shared_ptr<DirectionalLightComponent> GetPrimaryDirectionalLightForEditing() const;

		/** Punctual lights from components only (directional today; point/spot would add their components). */
		std::vector<Light> GatherLightsForView() const;

		/** Call when an actor's mesh components or shadow flags change; invalidates shadow-projector cache. */
		void RefreshShadowProjectorForActor(std::shared_ptr<Actor> actor);
		/** First actor that owns a GltfMeshComponent with project-shadow enabled (same order as scene iteration). */
		std::shared_ptr<Actor> GetShadowProjectorActor() const;

		/**
		 * Active skylight for IBL: enabled SkyLightComponent with highest SortPriority (tie: first in iteration).
		 * Searches Actors and PendingActors. Null if none.
		 */
		std::shared_ptr<SkyLightComponent> FindPrimarySkyLightComponent() const;
		/** Full HDR path for the primary skylight, or nullopt if no valid component override this frame. */
		std::optional<std::wstring> ResolvePrimarySkyLightHDRFullPath() const;
		/** 0 when no enabled primary skylight or empty HDR; else primary component IBL intensity (clamped >= 0). */
		float GetSkyLightIBLScale() const;

	private:
		WorldPrivate* d_ptr = nullptr;
	};
}
