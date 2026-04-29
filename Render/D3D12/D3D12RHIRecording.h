#pragma once
/**
 * D3D12 RHI recording / submit sequencing: thread-local exclusive depth (RHIBeginFrame, device init,
 * viewport resize, etc.) plus optional upload-bypass for RHICreate* upload paths. Deferred command
 * queue is flushed at RHIBeginFrame (same-thread MVP for future dedicated RHI thread).
 * When RenderCore::RHI_RegisterRHISubmissionThread is used (Engine RenderThread), recording/submit
 * must also run on that thread (see RHI/RHIThreadPolicy.h, IsInRHIThread).
 */
#include <functional>

namespace RenderCore
{
	/** Increment exclusive depth (nested allowed). Used by RHIBeginFrame, scoped init/resize/cleanup. */
	void D3D12RHI_EnterExclusiveRegion();
	void D3D12RHI_LeaveExclusiveRegion();
	bool D3D12RHI_IsExclusiveRegionActive();
	int32_t D3D12RHI_GetExclusiveRegionDepth();

	/** Upload helpers (InitializeBuffer/Texture) may record outside a frame; nest RAII around those paths. */
	void D3D12RHI_EnterUploadBypassRegion();
	void D3D12RHI_LeaveUploadBypassRegion();
	bool D3D12RHI_IsUploadBypassActive();

	/** True if TransitionResource / allocator obtain / list obtain is allowed on this thread. */
	bool D3D12RHI_IsRecordingAllowed();
	void D3D12RHI_CheckRecordingAllowed(const char* OperationLabel);

	/** True for ExecuteCommandLists, CommitTrackedResourceStateToGlobal, ReleaseCommandList, etc. */
	bool D3D12RHI_IsSubmitAllowed();
	void D3D12RHI_CheckSubmitAllowed(const char* OperationLabel);

	void D3D12RHI_EnqueueDeferredCommand(std::function<void()> Fn);
	void D3D12RHI_FlushDeferredCommands();

	struct D3D12RHI_ScopedExclusiveRegion
	{
		D3D12RHI_ScopedExclusiveRegion() { D3D12RHI_EnterExclusiveRegion(); }
		~D3D12RHI_ScopedExclusiveRegion() { D3D12RHI_LeaveExclusiveRegion(); }
	};

	struct D3D12RHI_ScopedUploadBypassRegion
	{
		D3D12RHI_ScopedUploadBypassRegion() { D3D12RHI_EnterUploadBypassRegion(); }
		~D3D12RHI_ScopedUploadBypassRegion() { D3D12RHI_LeaveUploadBypassRegion(); }
	};
}
