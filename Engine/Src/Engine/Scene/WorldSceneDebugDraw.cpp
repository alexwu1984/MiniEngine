#include "Scene/WorldSceneDebugDraw.h"
#include "Render/MaterialPreFrame.h"
#include "Render/Shadow/ShadowRenderPass.h"
#include <algorithm>

namespace Engine
{
	void WorldSceneDebugDraw::SetShowSceneMeshBoundsDebug(bool bIn)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		bShowSceneMeshBoundsDebug = bIn;
	}

	bool WorldSceneDebugDraw::GetShowSceneMeshBoundsDebug() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return bShowSceneMeshBoundsDebug;
	}

	void WorldSceneDebugDraw::SetShowShadowCasterMeshBoundsDebug(bool bIn)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		bShowShadowCasterMeshBoundsDebug = bIn;
	}

	bool WorldSceneDebugDraw::GetShowShadowCasterMeshBoundsDebug() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return bShowShadowCasterMeshBoundsDebug;
	}

	void WorldSceneDebugDraw::SetShowDirectionalCSMCascadeSubjectBoundsDebug(bool bIn)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		bShowDirectionalCSMCascadeSubjectBoundsDebug = bIn;
		if (!bIn)
			dirCascadeSubjectAabbDebugCount = 0;
	}

	bool WorldSceneDebugDraw::GetShowDirectionalCSMCascadeSubjectBoundsDebug() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return bShowDirectionalCSMCascadeSubjectBoundsDebug;
	}

	void WorldSceneDebugDraw::GetDirectionalCSMCascadeSubjectDebugCopy(int& OutCount, math::AABB3 OutBoxes[3]) const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		OutCount = (std::clamp)(dirCascadeSubjectAabbDebugCount, 0, 3);
		for (int i = 0; i < 3; ++i)
			OutBoxes[i] = dirCascadeSubjectWorldAabbDebug[i];
	}

	void WorldSceneDebugDraw::UpdateDirectionalCSMCascadeSubjectDebugFromShadowPass(const ShadowRenderPass* P)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!bShowDirectionalCSMCascadeSubjectBoundsDebug)
		{
			dirCascadeSubjectAabbDebugCount = 0;
			return;
		}
		if (!P)
		{
			dirCascadeSubjectAabbDebugCount = 0;
			return;
		}
		const CBDirectionalShadow& cb = P->GetCachedDirectionalShadow();
		if (cb.DirectionalCSMEnabled != 1)
		{
			dirCascadeSubjectAabbDebugCount = 0;
			return;
		}
		P->GetDirectionalCSMCascadeSubjectAABBs(dirCascadeSubjectAabbDebugCount, dirCascadeSubjectWorldAabbDebug);
	}

	void WorldSceneDebugDraw::ResetDirectionalCascadeSubjectOverlay(bool bClearShowFlag)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (bClearShowFlag)
			bShowDirectionalCSMCascadeSubjectBoundsDebug = false;
		dirCascadeSubjectAabbDebugCount = 0;
	}
}
