#pragma once
#include "Render/MaterialPreFrame.h"
#include "Render/ShadowDebugWireRenderer.h"
#include "math/aabb3.h"

namespace Engine
{
	class ShadowRenderPass;
	class World;

	/** Viewer / overlay debug toggles and transient shadow-debug snapshots. Kept off `World` public surface. */
	class WorldSceneDebugDraw
	{
	public:
		void SetShowSceneMeshBoundsDebug(bool bIn);
		bool GetShowSceneMeshBoundsDebug() const;

		void SetShowShadowCasterMeshBoundsDebug(bool bIn);
		bool GetShowShadowCasterMeshBoundsDebug() const;

		void SetShowDirectionalCSMCascadeSubjectBoundsDebug(bool bIn);
		bool GetShowDirectionalCSMCascadeSubjectBoundsDebug() const;
		void GetDirectionalCSMCascadeSubjectDebugCopy(int& OutCount, math::AABB3 OutBoxes[3]) const;
		void UpdateDirectionalCSMCascadeSubjectDebugFromShadowPass(const ShadowRenderPass* shadowPass);

		/**
		 * Appends per-light debug gizmos (directional arrow / spot cone / point sphere) into OutSubmit based on
		 * the latest cached shadow indices and per-component GetShowShadowFrustumDebug() flags. `ShadowPassLights`
		 * must be the same list submitted to the shadow pass (used to map cached light list index → per-type index).
		 */
		void CollectShadowDebugLightShapes(const World& WorldRef, ShadowRenderPass* ShadowPass,
										   const std::vector<Light>& ShadowPassLights, FShadowDebugWireSubmit& OutSubmit) const;

		/** Clears cascade-subject snapshot; optional cascade overlay flag (used on scene reset paths). */
		void ResetDirectionalCascadeSubjectOverlay(bool bClearShowFlag);

	private:
		mutable std::mutex mutex_{};
		bool bShowSceneMeshBoundsDebug = false;
		bool bShowShadowCasterMeshBoundsDebug = false;
		bool bShowDirectionalCSMCascadeSubjectBoundsDebug = false;
		int dirCascadeSubjectAabbDebugCount = 0;
		math::AABB3 dirCascadeSubjectWorldAabbDebug[3]{};
	};
}
