#pragma once
#include "Scene/DeviceInputState.h"
#include "Render/MaterialPreFrame.h"
#include "Render/Shadow/ShadowProjectorTypes.h"
#include "Render/SkyLightEnvironment.h"

namespace Engine
{
	class Actor;
	class CameraComponent;
	class SkyLightComponent;
	class DirectionalLightComponent;
	class PointLightComponent;
	class SpotLightComponent;
	class FScene;
	struct WorldPrivate;

	/** Game-thread world: actors, lights, main camera (UE world / scene subset; no input, no tick). */
	class World : public std::enable_shared_from_this<World>
	{
	public:
		World();
		~World();

		/** Spawn actors/lights from scene JSON (game content). Full scene swap is SceneManager::ReloadSceneJson. */
		void LoadScene(const std::wstring& ModelFile);
		/** First actor whose `GetActorName()` equals `Name` (scans Actors then PendingActors); null if none. */
		std::shared_ptr<Actor> FindFirstActorByName(const std::wstring& Name) const;
		/** After LoadScene when the world context changed: primary camera temporal / jitter / history (UE LocalPlayer ViewState analogue). */
		void InvalidatePrimaryViewStateAfterSceneCut();
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
		/** UE-style scene: primitive proxies and future scene-scoped render state (lifetime = this World). */
		std::shared_ptr<FScene> GetScene() const;
		/** Same ordering as the first entry in GatherLightsForView (highest SortPriority among enabled directionals). */
		std::shared_ptr<DirectionalLightComponent> GetPrimaryDirectionalLightForEditing() const;
		/** All enabled directionals, same sort order as GatherLightsForView (priority high → low). */
		std::vector<std::shared_ptr<DirectionalLightComponent>> GetDirectionalLightsForEditingSorted() const;
		/** All enabled point lights, same sort order as GatherLightsForView (priority high → low). */
		std::vector<std::shared_ptr<PointLightComponent>> GetPointLightsForEditingSorted() const;
		/** All enabled spot lights, same sort order as GatherLightsForView (priority high → low). */
		std::vector<std::shared_ptr<SpotLightComponent>> GetSpotLightsForEditingSorted() const;

		std::vector<Light> GatherLightsForView() const;

		/**
		 * True when GatherLightsForView applies procedural-sun placement to this spot
		 * (SyncProceduralSun / IsProceduralSunFill, or the single auto "sun key" shadow spot under procedural sky).
		 */
		bool DoesSpotUseProceduralSunKeyInGather(const std::shared_ptr<SpotLightComponent>& comp) const;

		/**
		 * Aim used by GatherLights procedural-sun placement. If the component aim is (0,0,0) (unset / default) and shadow
		 * projector bounds exist, returns (center.x, 0, center.z) so the cone targets the ground under shadow casters
		 * (AMD glTFSample-style outdoor framing). Otherwise returns the authored aim.
		 */
		math::Vector3 ResolveProceduralSunAimWorldForGather(const SpotLightComponent& comp) const;

		/** Call when an actor's mesh components or shadow flags change; invalidates shadow-projector cache. */
		void RefreshShadowProjectorForActor(std::shared_ptr<Actor> actor);
		/** First actor that owns a SceneMeshComponent with project-shadow enabled (same order as scene iteration). */
		std::shared_ptr<Actor> GetShadowProjectorActor() const;

		/** Union world AABB of every ProjShadow mesh (multi-caster); drives shadow cascade + fallback frustum. */
		FShadowProjectorSceneData BuildShadowProjectorAggregateData() const;

		/**
		 * Active skylight for IBL: enabled SkyLightComponent with highest SortPriority (tie: first in iteration).
		 * Searches Actors and PendingActors. Null if none.
		 */
		std::shared_ptr<SkyLightComponent> FindPrimarySkyLightComponent() const;
		/** Full HDR path for the primary skylight, or nullopt if no valid component override this frame. */
		std::optional<std::wstring> ResolvePrimarySkyLightHDRFullPath() const;
		/** Full skylight source description for this frame (procedural/file/none). Prefer over ResolvePrimarySkyLightHDRFullPath. */
		FSkyLightSourceDesc ResolvePrimarySkyLightSource() const;
		/** 0 when no enabled primary skylight or empty HDR; else primary component IBL intensity (clamped >= 0). */
		float GetSkyLightIBLScale() const;
		/** Primary skylight IBL environment rotation (degrees); (0,0) when no skylight. */
		void GetPrimarySkyLightIBLRotationDegrees(float& outPitchDeg, float& outYawDeg) const;

		/** True if current scene JSON listed a RoamCamera entry (after last LoadScene). */
		bool UsesRoamCameraScene() const;

	private:
		WorldPrivate* d_ptr = nullptr;
	};
}
