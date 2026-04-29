#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace Engine
{
	/**
	 * Dedicated thread that runs D3D12 GPU submission work forwarded from RHI_SubmitOrInline (B-tier queue).
	 * Each task runs under a per-submit D3D12RHI_ScopedExclusiveRegion on this thread.
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
