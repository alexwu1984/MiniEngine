#include "D3D12/D3D12RHIRecording.h"
#include "RHI/RHIThreadPolicy.h"
#include "core/inc.h"
#include <cstdint>
#include <deque>
#include <mutex>

namespace RenderCore
{
	namespace
	{
		thread_local int32_t TLS_D3D12RHIExclusiveDepth = 0;
		thread_local int32_t TLS_D3D12RHIUploadBypassDepth = 0;

		std::mutex GDeferredMutex;
		std::deque<std::function<void()>> GDeferredCommands;
	}

	void D3D12RHI_EnterExclusiveRegion()
	{
		++TLS_D3D12RHIExclusiveDepth;
	}

	void D3D12RHI_LeaveExclusiveRegion()
	{
		Assert(TLS_D3D12RHIExclusiveDepth > 0);
		--TLS_D3D12RHIExclusiveDepth;
	}

	bool D3D12RHI_IsExclusiveRegionActive()
	{
		return TLS_D3D12RHIExclusiveDepth > 0;
	}

	int32_t D3D12RHI_GetExclusiveRegionDepth()
	{
		return TLS_D3D12RHIExclusiveDepth;
	}

	void D3D12RHI_EnterUploadBypassRegion()
	{
		++TLS_D3D12RHIUploadBypassDepth;
	}

	void D3D12RHI_LeaveUploadBypassRegion()
	{
		Assert(TLS_D3D12RHIUploadBypassDepth > 0);
		--TLS_D3D12RHIUploadBypassDepth;
	}

	bool D3D12RHI_IsUploadBypassActive()
	{
		return TLS_D3D12RHIUploadBypassDepth > 0;
	}

	bool D3D12RHI_IsRecordingAllowed()
	{
		const bool bScopeOk = D3D12RHI_IsExclusiveRegionActive() || D3D12RHI_IsUploadBypassActive();
		if (!bScopeOk)
			return false;
		return RHI_IsInRHIRecordingThread();
	}

	void D3D12RHI_CheckRecordingAllowed(const char* OperationLabel)
	{
		ensureMsgf(D3D12RHI_IsRecordingAllowed(),
				   "D3D12 RHI: %s must run inside an exclusive region (RHIBeginFrame/RHIEndFrame pair, device init/cleanup, viewport resize, Wait/Shutdown) or D3D12RHI_ScopedUploadBypassRegion (upload helpers), and on the registered RHI recording thread when one is set.",
				   OperationLabel ? OperationLabel : "(unknown)");
	}

	bool D3D12RHI_IsSubmitAllowed()
	{
		const bool bScopeOk = D3D12RHI_IsExclusiveRegionActive() || D3D12RHI_IsUploadBypassActive();
		if (!bScopeOk)
			return false;
		return RHI_IsInRHISubmissionThread();
	}

	void D3D12RHI_CheckSubmitAllowed(const char* OperationLabel)
	{
		ensureMsgf(D3D12RHI_IsSubmitAllowed(),
				   "D3D12 RHI: %s must run inside an exclusive or upload-bypass region, and on the registered RHI submission thread when one is set.",
				   OperationLabel ? OperationLabel : "(unknown)");
	}

	void D3D12RHI_EnqueueDeferredCommand(std::function<void()> Fn)
	{
		if (!Fn)
			return;
		if (D3D12RHI_IsRecordingAllowed())
		{
			Fn();
			return;
		}
		std::lock_guard<std::mutex> Lock(GDeferredMutex);
		GDeferredCommands.push_back(std::move(Fn));
	}

	void D3D12RHI_FlushDeferredCommands()
	{
		std::deque<std::function<void()>> Local;
		{
			std::lock_guard<std::mutex> Lock(GDeferredMutex);
			Local.swap(GDeferredCommands);
		}
		for (std::function<void()>& Cmd : Local)
		{
			if (Cmd)
				Cmd();
		}
	}
}
