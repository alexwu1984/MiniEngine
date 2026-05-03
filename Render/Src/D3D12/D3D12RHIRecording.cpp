#include "D3D12/D3D12RHIRecording.h"
#include "RHI/RHIThreadPolicy.h"
#include "core/inc.h"
#include <vector>

namespace RenderCore
{
	namespace RecordingDetail
	{
		thread_local std::vector<ERHIRecordingContextScope> GRecordingContextStackTLS;

		inline bool DispatchRecordingWorkflowScopeActive()
		{
			return !GRecordingContextStackTLS.empty();
		}
	}

	void D3D12RHI_PushRecordingContext(ERHIRecordingContextScope Scope)
	{
		RecordingDetail::GRecordingContextStackTLS.push_back(Scope);
	}

	void D3D12RHI_PopRecordingContext()
	{
		Assert(!RecordingDetail::GRecordingContextStackTLS.empty());
		RecordingDetail::GRecordingContextStackTLS.pop_back();
	}

	void D3D12RHI_PopRecordingContextIfTopIs(ERHIRecordingContextScope Expected)
	{
		if (RecordingDetail::GRecordingContextStackTLS.empty())
			return;
		if (RecordingDetail::GRecordingContextStackTLS.back() != Expected)
			return;
		RecordingDetail::GRecordingContextStackTLS.pop_back();
	}

	bool D3D12RHI_IsRecordingContextStackActive()
	{
		return !RecordingDetail::GRecordingContextStackTLS.empty();
	}

	bool D3D12RHI_IsUploadBypassActive()
	{
		for (const ERHIRecordingContextScope Scope : RecordingDetail::GRecordingContextStackTLS)
		{
			if (Scope == ERHIRecordingContextScope::OutsideFrameResourceUpload)
			{
				return true;
			}
		}
		return false;
	}

	bool D3D12RHI_IsRecordingAllowed()
	{
		if (!RecordingDetail::DispatchRecordingWorkflowScopeActive())
		{
			return false;
		}
		return RHI_IsInRHIRecordingThread();
	}

	void D3D12RHI_CheckRecordingAllowed(const char* OperationLabel)
	{
		ensureMsgf(D3D12RHI_IsRecordingAllowed(),
				   "D3D12 RHI: %s must run inside D3D12RHI_ScopedRecordingContext with a matching ERHIRecordingContextScope "
				   "(InsideFrameTick after RHIBeginFrame, OutsideFrameResourceUpload for upload helpers, RHIFrameBoundary for "
				   "frame fences, etc.), and on the registered RHI recording thread when one is set.",
				   OperationLabel ? OperationLabel : "(unknown)");
	}

	bool D3D12RHI_IsSubmitAllowed()
	{
		if (!RecordingDetail::DispatchRecordingWorkflowScopeActive())
		{
			return false;
		}
		return RHI_IsInRHIExecutionThread();
	}

	void D3D12RHI_CheckSubmitAllowed(const char* OperationLabel)
	{
		ensureMsgf(D3D12RHI_IsSubmitAllowed(),
				   "D3D12 RHI: %s must run inside a non-empty recording context stack scope, "
				   "and on the registered RHI execution (submission) thread when one is set.",
				   OperationLabel ? OperationLabel : "(unknown)");
	}
}
