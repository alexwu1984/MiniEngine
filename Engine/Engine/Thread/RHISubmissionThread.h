#pragma once
#include "core/inc.h"

namespace Engine
{
	/**
	 * RHI execution thread (UE4 naming): sequential queue for D3D12 device work forwarded from RenderCore::
	 * ENQUEUE_RHI_SUBMIT_COMMAND / RHI_SubmitOrInline (GPU submit) and ENQUEUE_RHI_COMMAND / RHI_ExecuteDeferredOrInline (root / PSO Create*, etc.). Same FIFO preserves order.
	 * Each task runs under D3D12RHI_ScopedRecordingContext(SubmissionThreadTask) on this thread when used.
	 * Submission vs game recording thread: see Render/RenderQueueSynchronization.h.
	 */
	class RHISubmissionThread
	{
	public:
		RHISubmissionThread();
		~RHISubmissionThread();

		void Start();
		void Stop();

		/** Blocks until Work has finished on the submission thread. */
		void EnqueueAndWait(std::function<void()> Work);

	private:
		void RunLoop();

		std::thread Worker;
		std::mutex QueueMutex;
		std::condition_variable QueueCv;
		std::deque<std::function<void()>> Queue;
		std::atomic_bool StopFlag{ false };
	};
}
