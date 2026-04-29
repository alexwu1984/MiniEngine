#pragma once
#include <functional>
#include <thread>

namespace RenderCore
{
	/**
	 * Thread identity for D3D12: recording (RenderThread) vs submission (dedicated RHI thread when enabled).
	 * When a registration is absent, the corresponding check is permissive (tools / DemoRunner).
	 */
	void RHI_RegisterRHIRecordingThread(std::thread::id ThreadId);
	void RHI_UnregisterRHIRecordingThread();
	bool RHI_IsRHIRecordingThreadRegistered();
	bool RHI_IsInRHIRecordingThread();

	void RHI_RegisterRHISubmissionThread(std::thread::id ThreadId);
	void RHI_UnregisterRHISubmissionThread();
	bool RHI_IsRHISubmissionThreadRegistered();
	bool RHI_IsInRHISubmissionThread();

	inline bool IsInRHIThread()
	{
		return RHI_IsInRHISubmissionThread();
	}

	/** When set, GPU submit work from non-submission threads is forwarded here (must block until done). */
	void RHI_SetSubmissionExecutor(std::function<void(std::function<void()>)> Executor);
	void RHI_ClearSubmissionExecutor();

	/** Runs submit work on the submission thread when an executor is set and caller is not the submission thread; otherwise inline. */
	void RHI_SubmitOrInline(const char* OperationLabel, std::function<void()> Work);
}
