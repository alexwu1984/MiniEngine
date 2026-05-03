#include "RHI/RHIThreadPolicy.h"
#include "D3D12/D3D12RHIRecording.h"
#include <mutex>
#include <optional>

namespace RenderCore
{
	namespace
	{
		std::mutex GRecordingThreadMutex;
		std::optional<std::thread::id> GRecordingThreadId;

		std::mutex GSubmissionThreadMutex;
		std::optional<std::thread::id> GSubmissionThreadId;

		std::mutex GExecutorMutex;
		std::function<void(std::function<void()>)> GSubmissionExecutor;

		std::recursive_mutex G_D3D12QueueSubmitMutex;
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
		std::lock_guard<std::mutex> Lock(GRecordingThreadMutex);
		GRecordingThreadId = ThreadId;
	}

	void RHI_UnregisterRHIRecordingThread()
	{
		std::lock_guard<std::mutex> Lock(GRecordingThreadMutex);
		GRecordingThreadId.reset();
	}

	bool RHI_IsRHIRecordingThreadRegistered()
	{
		std::lock_guard<std::mutex> Lock(GRecordingThreadMutex);
		return GRecordingThreadId.has_value();
	}

	bool RHI_IsInRHIRecordingThread()
	{
		std::lock_guard<std::mutex> Lock(GRecordingThreadMutex);
		if (!GRecordingThreadId.has_value())
			return true;
		return std::this_thread::get_id() == *GRecordingThreadId;
	}

	void RHI_RegisterRHISubmissionThread(std::thread::id ThreadId)
	{
		std::lock_guard<std::mutex> Lock(GSubmissionThreadMutex);
		GSubmissionThreadId = ThreadId;
	}

	void RHI_UnregisterRHISubmissionThread()
	{
		std::lock_guard<std::mutex> Lock(GSubmissionThreadMutex);
		GSubmissionThreadId.reset();
	}

	bool RHI_IsRHISubmissionThreadRegistered()
	{
		std::lock_guard<std::mutex> Lock(GSubmissionThreadMutex);
		return GSubmissionThreadId.has_value();
	}

	bool RHI_IsInRHISubmissionThread()
	{
		std::lock_guard<std::mutex> Lock(GSubmissionThreadMutex);
		if (!GSubmissionThreadId.has_value())
			return true;
		return std::this_thread::get_id() == *GSubmissionThreadId;
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
