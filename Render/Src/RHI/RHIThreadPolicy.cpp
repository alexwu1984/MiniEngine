#include "RHI/RHIThreadPolicy.h"
#include "D3D12/D3D12RHIRecording.h"
#include <atomic>
#include <functional>
#include <mutex>

namespace RenderCore
{
	namespace
	{
		/** Registration mutex: guards std::function; thread ids use atomics (no lock on IsIn* hot path). */
		std::mutex GExecutorMutex;
		std::function<void(std::function<void()>)> GSubmissionExecutor;

		std::recursive_mutex G_D3D12QueueSubmitMutex;

		/** 0 = not registered (IsIn* is permissive / true). Otherwise std::thread::id fingerprint. */
		std::atomic<uint64_t> GRecordingThreadFingerprint{0};
		std::atomic<uint64_t> GSubmissionThreadFingerprint{0};

		uint64_t ThreadIdFingerprint(std::thread::id Tid)
		{
			const size_t H = std::hash<std::thread::id>{}(Tid);
			uint64_t V = static_cast<uint64_t>(H);
			if (V == 0)
				V = 1;
			return V;
		}
	}

	std::recursive_mutex& RHI_D3D12QueueSubmitMutex()
	{
		return G_D3D12QueueSubmitMutex;
	}

	RHI_D3D12ScopedQueueSubmitLock::RHI_D3D12ScopedQueueSubmitLock()
		: Guard(RHI_D3D12QueueSubmitMutex())
	{
	}

	void RHI_RegisterRHIRecordingThread(std::thread::id ThreadId)
	{
		GRecordingThreadFingerprint.store(ThreadIdFingerprint(ThreadId), std::memory_order_release);
	}

	void RHI_UnregisterRHIRecordingThread()
	{
		GRecordingThreadFingerprint.store(0, std::memory_order_release);
	}

	bool RHI_IsRHIRecordingThreadRegistered()
	{
		return GRecordingThreadFingerprint.load(std::memory_order_acquire) != 0;
	}

	bool RHI_IsInRHIRecordingThread()
	{
		const uint64_t Expected = GRecordingThreadFingerprint.load(std::memory_order_acquire);
		if (Expected == 0)
			return true;
		return ThreadIdFingerprint(std::this_thread::get_id()) == Expected;
	}

	void RHI_RegisterRHISubmissionThread(std::thread::id ThreadId)
	{
		GSubmissionThreadFingerprint.store(ThreadIdFingerprint(ThreadId), std::memory_order_release);
	}

	void RHI_UnregisterRHISubmissionThread()
	{
		GSubmissionThreadFingerprint.store(0, std::memory_order_release);
	}

	bool RHI_IsRHISubmissionThreadRegistered()
	{
		return GSubmissionThreadFingerprint.load(std::memory_order_acquire) != 0;
	}

	bool RHI_IsInRHISubmissionThread()
	{
		const uint64_t Expected = GSubmissionThreadFingerprint.load(std::memory_order_acquire);
		if (Expected == 0)
			return true;
		return ThreadIdFingerprint(std::this_thread::get_id()) == Expected;
	}

	void RHI_SetSubmissionExecutor(std::function<void(std::function<void()>)> Executor)
	{
		std::lock_guard<std::mutex> Lock(GExecutorMutex);
		GSubmissionExecutor = std::move(Executor);
	}

	void RHI_ClearSubmissionExecutor()
	{
		std::lock_guard<std::mutex> Lock(GExecutorMutex);
		GSubmissionExecutor = nullptr;
	}

	void RHI_SubmitOrInline(const char* OperationLabel, std::function<void()> Work)
	{
		if (!Work)
			return;
		if (RHI_IsInRHISubmissionThread())
		{
			D3D12RHI_CheckSubmitAllowed(OperationLabel);
			Work();
			return;
		}
		std::function<void(std::function<void()>)> Exec;
		{
			std::lock_guard<std::mutex> Lock(GExecutorMutex);
			Exec = GSubmissionExecutor;
		}
		if (Exec)
		{
			Exec(std::move(Work));
			return;
		}
		D3D12RHI_CheckSubmitAllowed(OperationLabel);
		Work();
	}

	void RHI_ExecuteDeferredOrInline(const char* OperationLabel, std::function<void()> Work)
	{
		(void)OperationLabel;
		if (!Work)
			return;
		if (RHI_IsInRHISubmissionThread())
		{
			Work();
			return;
		}
		std::function<void(std::function<void()>)> Exec;
		{
			std::lock_guard<std::mutex> Lock(GExecutorMutex);
			Exec = GSubmissionExecutor;
		}
		if (Exec)
		{
			Exec(std::move(Work));
			return;
		}
		Work();
	}
}
