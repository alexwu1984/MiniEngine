#pragma once
#include "Engine/Thread/EngineThread.h"
#include "Render/RenderQueueSynchronization.h"

namespace RenderCore
{
	class DynamicRHI;
}

namespace Engine
{
	struct RenderThreadPrivate;

	/**
	 * Executes queued lambdas that record/submit GPU work. Registers the current std::thread::id with
	 * RenderCore::RHI_RegisterRHIRecordingThread for D3D12 recording checks; GPU submit and deferred Create* run on RHISubmissionThread (RHI execution thread).
	 */
	class RenderThread
	{
	public:
		RenderThread(RenderCore::DynamicRHI* DyRHI);
		~RenderThread();

		void Start();
		void Stop();

		void AppendCommand(std::function<void(RenderCore::DynamicRHI*)> fun);
		void WaitForFinish();
		/** Render worker thread id after Start(); default id if worker is not running. */
		std::thread::id GetWorkerThreadId() const;

	private:
		void Run();
		/** Recording worker only: run CommandQueue commands stolen mid-batch so Flush drains cross-thread work without waiting on DrainWait (avoids deadlock with nested ENQUEUE wait=true). */
		void PumpRecordingQueueUntilEmpty();

	private:
		RenderThreadPrivate* d_ptr = nullptr;
	};

	extern RenderThread* GRenderThread;

	/**
	 * Drains the FIFO on the recording worker until the queue is empty and the worker is idle (may span multiple swap batches) — not RHI GPU idle.
	 * Prefer the ERenderQueueFlushCategory overload for new code; see RenderQueueSynchronization.h.
	 */
	void FlushRenderingCommands();
	void FlushRenderingCommands(ERenderQueueFlushCategory Category);

	/**
	 * Enqueue work on the render thread. Prefer enqueue + categorized FlushRenderingCommands over wait=true when you want UE-like separation
	 * between recording (lambda) and draining the queue. wait=true is equivalent to enqueue followed immediately by WaitForFinish().
	 */
	void ENQUEUE_UNIQUE_RENDER_COMMAND(std::function<void(RenderCore::DynamicRHI*)> fun, bool wait = false);
}
