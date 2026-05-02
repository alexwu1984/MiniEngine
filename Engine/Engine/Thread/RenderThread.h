#pragma once
#include "Engine/Thread/EngineThread.h"

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

	private:
		RenderThreadPrivate* d_ptr = nullptr;
	};

	extern RenderThread* GRenderThread;

	/**
	 * Blocks until the render worker drains queued lambdas through its current batch boundary (does not imply GPU idle if submit is deferred).
	 * UE analogue: the narrow subset of FlushRenderingCommands() that maps to our single render-queue fence.
	 */
	inline void FlushRenderingCommands()
	{
		if (GRenderThread)
			GRenderThread->WaitForFinish();
	}

	/**
	 * Enqueue work on the render thread. Prefer enqueue + FlushRenderingCommands() over wait=true when you want UE-like separation
	 * between recording (lambda) and the fence (flush). wait=true is equivalent to enqueue followed immediately by FlushRenderingCommands().
	 */
	void ENQUEUE_UNIQUE_RENDER_COMMAND(std::function<void(RenderCore::DynamicRHI*)> fun, bool wait = false);
}