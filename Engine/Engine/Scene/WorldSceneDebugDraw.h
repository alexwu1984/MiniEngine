#pragma once
#include "math/aabb3.h"
#include <mutex>

namespace Engine
{
	class ShadowRenderPass;

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
