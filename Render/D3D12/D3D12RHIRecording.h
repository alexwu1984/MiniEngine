#pragma once
/**
 * D3D12 recording / submit gating uses a single per-thread ERHIRecordingContextScope stack (“path carries the contract”).
 * RHIBeginFrame / RHIEndFrame pairing uses Push in Begin + Pop-if-top matching in End because it cannot span one RAII object.
 * Pipeline contract table: RHI/RHIPathContracts.h (Phase A).
 * Recording vs RHI execution: RHI/RHIThreadPolicy.h (ENQUEUE_RHI_SUBMIT_COMMAND, ENQUEUE_RHI_COMMAND, …).
 */
#include "RHI/RHIPathContracts.h"
#include <cstdint>

namespace RenderCore
{
	enum class ERHIRecordingContextScope : uint8_t
	{
		/** Destructor runs before RHIEndFrame (frame recording body), after RHIBeginFrame returns. */
		InsideFrameTick = 0,
		/** RHICreate* uploads: InitializeTexture / InitializeBuffer when not inside InsideFrameTick. */
		OutsideFrameResourceUpload,
		/** D3D12DynamicRHI::RHIBeginFrame / RHIEndFrame pairing (Push in BeginFrame, conditional Pop in EndFrame). */
		RHIFrameBoundary,
		SubmissionThreadTask,
		/** Swap chain create / resize / teardown paths on the viewport thread. */
		SwapChainMaintenance,
		/** FD3D12Device ctor-style init and Cleanup teardown. */
		DeviceLifetimeBatch,
		/** Gpu idle / teardown (viewport teardown, allocator drain, Shutdown path, …). */
		GpuDrainIdle,
	};

	void D3D12RHI_PushRecordingContext(ERHIRecordingContextScope Scope);
	void D3D12RHI_PopRecordingContext();
	/** Removes one stack entry only if stack top equals Expected (for RHIFrameBoundary when BeginFrame took an early-out). */
	void D3D12RHI_PopRecordingContextIfTopIs(ERHIRecordingContextScope Expected);
	/** True when the current thread has any pushed recording context (nested allowed). */
	bool D3D12RHI_IsRecordingContextStackActive();

	/** True if any context on the stack authorizes “upload-style” work (subset for diagnostics / future policy). */
	bool D3D12RHI_IsUploadBypassActive();

	/** True if TransitionResource / allocator obtain / list obtain is allowed on this thread. */
	bool D3D12RHI_IsRecordingAllowed();
	void D3D12RHI_CheckRecordingAllowed(const char* OperationLabel);

	/** True for ExecuteCommandLists, CommitTrackedResourceStateToGlobal, ReleaseCommandList, etc. */
	bool D3D12RHI_IsSubmitAllowed();
	void D3D12RHI_CheckSubmitAllowed(const char* OperationLabel);

	struct D3D12RHI_ScopedRecordingContext
	{
		explicit D3D12RHI_ScopedRecordingContext(ERHIRecordingContextScope InScope)
		{
			D3D12RHI_PushRecordingContext(InScope);
		}
		~D3D12RHI_ScopedRecordingContext()
		{
			D3D12RHI_PopRecordingContext();
		}
		D3D12RHI_ScopedRecordingContext(const D3D12RHI_ScopedRecordingContext&) = delete;
		D3D12RHI_ScopedRecordingContext& operator=(const D3D12RHI_ScopedRecordingContext&) = delete;
	};
}
