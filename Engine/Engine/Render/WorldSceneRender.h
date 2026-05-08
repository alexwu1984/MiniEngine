#pragma once
#include "core/inc.h"
#include "core/event.h"
#include "core/color.h"
#include "math/matrix4x4.h"
#include "math/vector3.h"
#include "Render/RDGBuilder.h"
#include "tinygltf/json.h"

namespace RenderCore
{
	class DynamicRHI;
	class RHICommandContext;
	class RHIViewPort;
}

namespace Engine
{
	class PreProcessor;
	class PostProcessor;
	class World;
	struct FWorldSceneRenderPrivate;
	class ShadowRenderPass;
	class CubeBackground;

	/**
	 * Long-lived viewport + render pipeline owner (textures, preprocess, shadows, deferred, postprocess).
	 * Current World/FScene resolve each frame via a non-owning world ref; scene swap only rebinds that ref (UE-style SetWorld naming).
	 * Paired with FSceneRenderer: each tick enqueues an FSceneRenderPacket (FIFO) without Flush — simulation can overlap recording.
	 */
	class FWorldSceneRender : public std::enable_shared_from_this<FWorldSceneRender>
	{
	public:
		FWorldSceneRender(std::weak_ptr<World> Owner);
		~FWorldSceneRender();
		std::shared_ptr<World> GetWorld() const;
		/** Non-owning association (std::weak_ptr); matches UE viewport “SetWorld(InWorld)” without spelling out storage. */
		void SetWorld(std::weak_ptr<World> InWorld);

		void InitResource(std::shared_ptr<RenderCore::RHIViewPort> ViewPort);
		void LoadConfig(const nlohmann::json& Root);
		void SetBackgroundColor(const core::FLinearColor& Color);
		void Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen);
		void Render(float DeltaTime);
		std::shared_ptr<PreProcessor> GetPreProcessor() const;
		std::shared_ptr<PostProcessor> GetPostProcessor() const;
		std::shared_ptr<ShadowRenderPass> GetShadowRenderPass() const;
		std::shared_ptr<RenderCore::RHIViewPort> GetViewPort() const;

		/**
		 * UE Renderer batch: shadow pass persistent caches + recycle viewport-sized scene targets + invalidate temporal post-process state.
		 * Call from game thread after LoadScene when the logical world behind the viewport changed; pairs with World::InvalidatePrimaryViewStateAfterSceneCut.
		 */
		void NotifyWorldRenderingSceneChanged();

		/**
		 * UE426 analogue: flush mesh-material draw cache only when replacing World (ReloadSceneJson), before old FScene teardown.
		 * Pointer-keyed caches are not cleared on RemoveActor; primitives are re-gathered each frame from FScene.
		 */
		void FlushClearMeshMaterialRenderCacheNow();
		/** Before destroying the current World: drop shadow pass mesh→ShadowPS cache so old meshes are not kept alive and PSOs are not reused across scene cuts. */
		void FlushClearShadowPassMeshCacheNow();

		/** 0 = do not throttle; otherwise block game thread via Flush until pending ExecuteFrame jobs drop below Max. Parsed from MainEngine / command line. */
		void SetMaxSceneFramesInFlight(uint32_t MaxConcurrent) noexcept;

		/** End-of-tick coupling: rendersync drains the game-thread render-queue; gpuwait additionally blocks on RHI GPU-idle (DynamicRHI::RHIWaitForGpuIdle). Game thread only. */
		void EndGameThreadFrameSync(bool bFlushRenderQueue, bool bGpuIdleWait);

		/** Ordinal of full-frame submits enqueued from the game thread (0 = none yet). GPU completion is expressed by RHI, not by this counter. */
		uint64_t GetSubmissionSequence() const noexcept;
		/** ExecuteFrame jobs queued on the recording thread but not finished (lambda still running). Paired with maxrenderframes / GetMaxSceneFramesInFlight. */
		uint32_t GetPendingSceneFramesCount() const noexcept;
		/** Current maxrenderframes cap (GetMaxSceneFramesInFlight / SetMaxSceneFramesInFlight); 0 = unlimited. */
		uint32_t GetMaxSceneFramesInFlight() const noexcept;

		void SetShowDirectionalLightFrustum(bool bEnable) noexcept;
		bool GetShowDirectionalLightFrustum() const noexcept;
		/** Thread-safe copy of last completed frame's RDG pass CPU Encode timings (see HUD disclaimer). */
		void GetLastFramePassCpuTimings(std::vector<FRDGPassCpuTiming>& Out) const;
		/** Valid after Shadow RDG pass when directional shadow was rendered this frame (render-thread matrices). */
		bool TryGetGuiDebugDirLightFrustum(math::Matrix4x4& OutLightViewProj, math::Vector3& OutLightDirectionTowardSource, math::Matrix4x4& OutCameraViewProj) const noexcept;

	private:
		/** Game thread: gather views/primitives, enqueue FSceneRenderPacket for SceneRenderer::ExecuteFrame (no per-tick Flush). */
		void SubmitSceneForRendering(float DeltaTime);

	public:
		core::event<void()> sigGuiEvent;

	private:
		/** Owned by `FWorldSceneRender`; render-queue lambdas that need async lifetime pin `shared_from_this()`, not shared ownership of impl. */
		std::unique_ptr<FWorldSceneRenderPrivate> d_ptr;
	};
} // namespace Engine
