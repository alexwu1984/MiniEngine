#pragma once
#include "Render/MaterialPreFrame.h"
#include "Render/Shadow/ShadowProjectorTypes.h"
#include "math/aabb3.h"
#include <vector>

namespace Engine
{
	struct GltfSceneMeshInfo;

	/**
	 * UE-style: which lights in the frame list own the single whole-scene shadow slots (decoupled from ad-hoc ShadowMapIndex usage at call sites).
	 * Indices are into the same `std::vector<Light>` passed to the shadow pass for this view.
	 */
	struct FShadowLightSlotBindings
	{
		int DirectionalLightListIndex = -1;
		int PointCubeShadowLightListIndex = -1;
		int SpotShadowLightListIndex = -1;
	};

	/**
	 * UE-style read-only snapshot of shadow-relevant view inputs for one frame (meshes + projector + merged bounds + slot bindings).
	 * Non-owning pointers — lifetime must cover shadow raster work on the render thread / RHI context for that frame.
	 */
	struct FShadowViewData
	{
		const std::vector<GltfSceneMeshInfo>* ShadowCasterMeshes = nullptr;
		const std::vector<GltfSceneMeshInfo>* FrustumBoundsMeshes = nullptr;
		/** Lights for this frame; passes may patch ShadowMapIndex / matrices on entries (same as prior behavior). */
		std::vector<Light>* FrameLights = nullptr;

		FShadowProjectorSceneData ProjectorScene{};
		const std::vector<GltfSceneMeshInfo>* SubjectMeshListForFrustum = nullptr;

		math::AABB3 SubjectWorldAabb{};
		math::AABB3 ReceiverWorldAabb{};
		bool bSubjectValid = false;
		bool bReceiverValid = false;

		FShadowLightSlotBindings LightSlots{};

		/** Fills bounds + slot indices; does not touch RHI. Caller should run mesh-drawer prune before/after as needed. */
		static FShadowViewData Build(const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const std::vector<GltfSceneMeshInfo>& FrustumBoundsMeshes,
								   std::vector<Light>& Lights, const FShadowProjectorSceneData& ShadowProjectorScene);
	};
}
