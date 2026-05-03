#pragma once
#include "core/inc.h"

namespace RenderCore
{
	/**
	 * D3D12 thread model (UE4-style names):
	 *
	 * - RHI recording thread: builds command lists (Engine::RenderThread when registered). See RHI_IsInRHIRecordingThread().
	 *
	 * - RHI execution thread: FIFO worker (Engine::RHISubmissionThread) runs ExecuteCommandLists / heavy submit. Recording stays
	 *   on RenderThread so CPU overlaps GPU. ID3D12CommandQueue is **not** thread-safe: any Execute / Signal / queue Wait must be
	 *   serialized — see RHI_D3D12ScopedQueueSubmitLock in D3D12DirectCommandListManager (covers RHISubmitThread vs game-thread idle).
	 *
	 * Submission executor storage uses one mutex; recording vs submission membership is hinted with atomic thread fingerprints
	 * (no mutex on every RHI_IsIn* query). Separate recursive mutex guards queue Execute/Signal/Wait batches.
	 * When no submission thread is registered (`-norhithread`), checks are permissive (DemoRunner); work runs inline on caller.
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

	/** Coarse lock for all ID3D12CommandQueue Execute / Signal / Wait usage (cross-thread safe with RHISubmissionThread). */
	std::recursive_mutex& RHI_D3D12QueueSubmitMutex();

	/** RAII — prefer wrapping each D3D queue submission batch (recursive: nested submits allowed). */
	struct RHI_D3D12ScopedQueueSubmitLock
	{
		std::lock_guard<std::recursive_mutex> Guard;
		RHI_D3D12ScopedQueueSubmitLock();
	};

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
