#pragma once
#include "Render/MaterialPreFrame.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "Render/Shadow/ShadowProjectorTypes.h"
#include "Render/SkyLightEnvironment.h"
#include "Scene/SceneMeshComponent.h"

namespace RenderCore
{
	class DynamicRHI;
}

namespace Engine
{
	class FWorldSceneRender;
	struct FWorldSceneRenderPrivate;
	class FScene;

	/**
	 * Per-frame inputs for RDG execution (game thread builds, render thread consumes).
	 * Queued with the render command so the game thread does not need Flush before the next Submit —
	 * avoids overwriting a single-slot "Submit then Render" mailbox.
	 */
	struct FSceneRenderPacket
	{
		FWorldSceneRender* WorldSceneRenderOwner = nullptr;
		FWorldSceneRenderPrivate* SceneResources = nullptr;
		std::shared_ptr<FScene> WorldScene;
		std::shared_ptr<const FSceneViewData> ViewData;
		std::vector<GltfSceneMeshInfo> MeshesInfo;
		std::vector<GltfSceneMeshInfo> ShadowCasters;
		std::vector<GltfSceneMeshInfo> ShadowFrustumBounds;
		std::vector<Light> LightsForShadow;
		FShadowProjectorSceneData ShadowProjectorScene{};
		FSkyLightSourceDesc SkyLightSource{};
		/** Matches FWorldSceneRenderPrivate::SubmissionSequence for this enqueue; carried so ExecuteFrame / profiling need not dereference viewport state. Starts at 1 for the first queued frame. */
		uint64_t SubmissionSequence = 0;
	};

	/**
	 * Records one frame on the render thread from an FSceneRenderPacket (no persistent per-frame scratch).
	 * Owned by FWorldSceneRenderPrivate; long-lived scene resources stay outside the packet.
	 */
	class FSceneRenderer
	{
	public:
		FSceneRenderer() = default;

		void ExecuteFrame(RenderCore::DynamicRHI* RHI, FSceneRenderPacket&& Packet);
	};
} // namespace Engine
