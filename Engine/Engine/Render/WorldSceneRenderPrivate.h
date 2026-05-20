#pragma once
#include "core/color.h"
#include "math/matrix4x4.h"
#include "math/vector3.h"
#include "Render/RDGBuilder.h"
#include "Render/SceneRendering/SceneRenderer.h"
#include "Render/GBufferVisualization.h"
#include "Render/ShadowDebugWireRenderer.h"

namespace RenderCore
{
	class RHIViewPort;
}

namespace Engine
{
	class World;
	class USkyLightComponent;
	class PostProcessor;
	class SkyLightRenderPass;
	class FSceneTextures;
	class ShadowRenderPass;
	class DeferredLightingPass;

	struct FWorldSceneRenderPrivate
	{
		std::weak_ptr<World> Owner;
		std::shared_ptr<RenderCore::RHIViewPort> MainViewPort;
		std::shared_ptr<USkyLightComponent> SkylightEnvironment;
		std::shared_ptr<PostProcessor> PostProcess;
		std::shared_ptr<SkyLightRenderPass> SkyLightPass;
		std::shared_ptr<FSceneTextures> SceneTextures;
		std::shared_ptr<ShadowRenderPass> ShadowRender;
		std::shared_ptr<FShadowDebugWireRenderer> ShadowDebugWire;
		std::shared_ptr<DeferredLightingPass> DeferredLighting;
		std::shared_ptr<FGBufferVisualizationPass> GBufferVisualization;
		FGBufferVisualizationSettings GBufferVisualizationSettings{};
		std::atomic_bool IsInit{ false };
		core::FLinearColor Color = core::FLinearColor::Blue;
		FRDGCompileParameters RDGCompileParams{};

		bool bUnlit = false;

		FSceneRenderer SceneRenderer;
		std::mutex RenderFrameMutex;

		/** Game-thread-only monotonic ordinal for queued full-frame ExecuteFrame submits (≠ GPU completion — see RHI present / gpuwait). Tools / overlap sizing. */
		std::atomic<uint64_t> SubmissionSequence{ 0 };

		/** Pending full scene ExecuteFrame jobs not yet finished on render thread (decrement when lambda returns). maxrenderframes throttles on game thread before enqueue. */
		std::atomic<uint32_t> PendingSceneFrames{ 0 };
		/** Upper bound on concurrent PendingSceneFrames before game thread waits (recording-queue drain). 0 = unlimited. Prefer aligning with DynamicRHI::RHIRecommendedParallelFrameResourceSlots for transient rings. Default from MainEngine (CLI or RHI). */
		std::atomic<uint32_t> MaxSceneFramesInFlight{ 2 };

		/** Filled during RDG ExecutePasses (render thread); copied to LastFramePassCpuTimingsForGui after the graph completes. */
		std::vector<FRDGPassCpuTiming> ScratchPassCpuTimings;
		mutable std::mutex PassCpuTimingMutex;
		std::vector<FRDGPassCpuTiming> LastFramePassCpuTimingsForGui;

		/** UE-style shadow frustum / point-bounds wire overlay (filled after Shadow RDG pass). */
		FShadowDebugWireSubmit ShadowDebugSubmit{};
	};
} // namespace Engine
