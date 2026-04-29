#pragma once
#include <functional>
#include <thread>

namespace RenderCore
{
	/**
	 * D3D12 thread model (UE4-style names):
	 *
	 * - RHI recording thread: builds command lists (Engine::RenderThread when registered). See RHI_IsInRHIRecordingThread().
	 *
	 * - RHI execution thread: runs sequential deferred device work (root signature / PSO Create*, and GPU submit when the
	 *   worker is enabled). In MiniEngine this is the same OS thread as Engine::RHISubmissionThread — registration uses
	 *   RHI_RegisterRHISubmissionThread / RHI_RegisterRHIExecutionThread (aliases). Tasks share one FIFO queue with
	 *   ENQUEUE_RHI_SUBMIT_COMMAND / RHI_SubmitOrInline so ordering matches UE’s “single RHI consumer” idea.
	 *
	 * When no submission/execution thread is registered, checks are permissive (tools / DemoRunner); executors unset
	 * means deferred work runs inline on the caller.
	 */
	void RHI_RegisterRHIRecordingThread(std::thread::id ThreadId);
	void RHI_UnregisterRHIRecordingThread();
	bool RHI_IsRHIRecordingThreadRegistered();
	bool RHI_IsInRHIRecordingThread();

	void RHI_RegisterRHISubmissionThread(std::thread::id ThreadId);
	void RHI_UnregisterRHISubmissionThread();
	bool RHI_IsRHISubmissionThreadRegistered();
	bool RHI_IsInRHISubmissionThread();

	/** True on the RHI execution thread (same as submission worker when multi-threaded D3D12 submit is enabled). */
	inline bool RHI_IsInRHIExecutionThread()
	{
		return RHI_IsInRHISubmissionThread();
	}

	inline void RHI_RegisterRHIExecutionThread(std::thread::id ThreadId)
	{
		RHI_RegisterRHISubmissionThread(ThreadId);
	}
	inline void RHI_UnregisterRHIExecutionThread()
	{
		RHI_UnregisterRHISubmissionThread();
	}
	inline bool RHI_IsRHIExecutionThreadRegistered()
	{
		return RHI_IsRHISubmissionThreadRegistered();
	}

	/** When set, work from non-execution threads is forwarded here (must block until done). Used for submit and deferred create. */
	void RHI_SetSubmissionExecutor(std::function<void(std::function<void()>)> Executor);
	void RHI_ClearSubmissionExecutor();

	/** GPU submit: must satisfy D3D12RHI_CheckSubmitAllowed when executed. Prefer ENQUEUE_RHI_SUBMIT_COMMAND at call sites. */
	void RHI_SubmitOrInline(const char* OperationLabel, std::function<void()> Work);

	/**
	 * Low-level FRHI-style path (UE4.26: immediate flush to RHI thread). Prefer ENQUEUE_RHI_COMMAND in call sites.
	 * Runs CreateRootSignature / Create*PipelineState / similar on the RHI execution thread when configured; otherwise
	 * inline. Does not assert submit/recording policy — callers must only enqueue device-safe work.
	 */
	void RHI_ExecuteDeferredOrInline(const char* OperationLabel, std::function<void()> Work);
}

/**
 * UE4.26-style RHI command macro: enqueues a [&](){ ... } block on the RHI execution thread FIFO
 * (same FIFO as ENQUEUE_RHI_SUBMIT_COMMAND / RHI_SubmitOrInline). Uses __VA_ARGS__ so commas in statements are safe;
 * do not pass a raw second-argument lambda (capture-list commas break the preprocessor).
 *
 * @param CmdName     Unquoted token, stringified for labels (e.g. RootSignatureFinalize).
 * @param __VA_ARGS__ Statements executed on the RHI thread (terminate with semicolons as usual).
 *
 * Example:
 *   ENQUEUE_RHI_COMMAND(RootSignatureFinalize,
 *       ok = Sig->Finalize(name, flags);
 *   );
 */
#define ENQUEUE_RHI_COMMAND(CmdName, ...) \
	::RenderCore::RHI_ExecuteDeferredOrInline("RHICommand/" #CmdName, [&]() { __VA_ARGS__ })

/**
 * UE4.26-style submit macro: forwards to RHI_SubmitOrInline. Same __VA_ARGS__ rules as ENQUEUE_RHI_COMMAND.
 *
 * Example:
 *   ENQUEUE_RHI_SUBMIT_COMMAND(FlushCommands_ExecutePending,
 *       Device->ExecutePendingCommandLists(QueueType, WaitForCompletion);
 *   );
 */
#define ENQUEUE_RHI_SUBMIT_COMMAND(CmdName, ...) \
	::RenderCore::RHI_SubmitOrInline("RHICommand_Submit/" #CmdName, [&]() { __VA_ARGS__ })
