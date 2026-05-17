#pragma once
#include "core/inc.h"

namespace Engine
{
	/**
	 * Documents the split between the game thread, the recording worker (RenderThread FIFO), and optional RHI submit work (D3D12).
	 * Use FlushRenderingCommands(Category) to drain recording; use DynamicRHI::RHIFlushSubmissionPipeline / RHIWaitForGpuIdle for the execution path.
	 * “Queued scene frames” (PendingSceneFrames / maxrenderframes) count ExecuteFrame jobs, not GPU completion.
	 */
	enum class ERenderQueueFlushCategory : uint8_t
	{
		/** Legacy / not yet audited — prefer migrating call sites to a specific category. */
		Unspecified = 0,

		/** Stop render thread, world teardown paths that release GPU proxies — drain before freeing refs. */
		LifetimeOrShutdown,

		/** Reload scene / swap world — drain around structural changes plus optional RHI gpu idle elsewhere. */
		ReloadOrWorldSwap,

		/** maxrenderframes throttling — cap queued ExecuteFrame jobs; not a gpuwait. */
		ThrottleQueuedSceneFrames,

		/** rendersync: end-of-tick policy (“recording queue caught up”). */
		PolicyEndOfTickRendersync,

		/** Cache / viewport invalidate commands must be observed on worker. */
		InvalidateRenderCaches,

		/** Material / texture creation on worker must finish before handles are read on game thread. */
		LoadOrResourceCreationSync,

		/** Deliberate sync immediately after enqueue (same intent as ENQUEUE ..., wait=true). */
		ExplicitSyncAfterEnqueue,
	};

} // namespace Engine
