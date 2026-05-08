#include "D3D12/D3D12RHIRecording.h"
#include "RHI/RHIThreadPolicy.h"
#include "core/inc.h"

namespace RenderCore
{
	namespace RecordingDetail
	{
		// Single thread_local aggregate: MSVC has had edge cases with multiple thread_local primitives in one TU
		// (stale/garbage depth when another slot is touched first, e.g. game-thread RHIWaitForGpuIdle during reload).
		static constexpr size_t kMaxRecordingContextDepth = 64u;
		struct FrameRecordingTLS
		{
			std::array<ERHIRecordingContextScope, kMaxRecordingContextDepth> Stack{};
			size_t Depth = 0;
		};

		inline FrameRecordingTLS& TLS()
		{
			thread_local FrameRecordingTLS Inst;
			return Inst;
		}

		inline void SanitizeDepthIfCorrupt(const char* Where)
		{
			FrameRecordingTLS& T = TLS();
			if (T.Depth > kMaxRecordingContextDepth)
			{
				ensureMsgf(false,
						   "D3D12 RHI: recording stack depth corrupted (%zu) at %s — resetting. "
						   "Usually extra Pop on this thread (Release asserts stripped) or TLS init ordering.",
						   T.Depth,
						   Where ? Where : "?");
				T.Depth = 0;
			}
		}

		inline bool DispatchRecordingWorkflowScopeActive()
		{
			SanitizeDepthIfCorrupt("DispatchRecordingWorkflowScopeActive");
			return TLS().Depth > 0u;
		}
	}

	void D3D12RHI_PushRecordingContext(ERHIRecordingContextScope Scope)
	{
		RecordingDetail::SanitizeDepthIfCorrupt("PushRecordingContext");
		RecordingDetail::FrameRecordingTLS& T = RecordingDetail::TLS();
		Assert(T.Depth < RecordingDetail::kMaxRecordingContextDepth);
		T.Stack[T.Depth++] = Scope;
	}

	void D3D12RHI_PopRecordingContext()
	{
		RecordingDetail::SanitizeDepthIfCorrupt("PopRecordingContext");
		RecordingDetail::FrameRecordingTLS& T = RecordingDetail::TLS();
		if (T.Depth == 0u)
		{
			// Release: Assert elided — extra Pop would underflow size_t and destroy TLS state (reload / GpuIdle paths).
			ensureMsgf(false, "D3D12 RHI: PopRecordingContext with empty stack (double Pop or mismatched Begin/End).");
			return;
		}
		--T.Depth;
	}

	void D3D12RHI_PopRecordingContextIfTopIs(ERHIRecordingContextScope Expected)
	{
		RecordingDetail::SanitizeDepthIfCorrupt("PopRecordingContextIfTopIs");
		RecordingDetail::FrameRecordingTLS& T = RecordingDetail::TLS();
		if (T.Depth == 0u)
			return;
		if (T.Stack[T.Depth - 1u] != Expected)
			return;
		--T.Depth;
	}

	bool D3D12RHI_IsRecordingContextStackActive()
	{
		RecordingDetail::SanitizeDepthIfCorrupt("IsRecordingContextStackActive");
		return RecordingDetail::TLS().Depth > 0u;
	}

	bool D3D12RHI_IsUploadBypassActive()
	{
		RecordingDetail::SanitizeDepthIfCorrupt("IsUploadBypassActive");
		const RecordingDetail::FrameRecordingTLS& T = RecordingDetail::TLS();
		for (size_t i = 0; i < T.Depth; ++i)
		{
			if (T.Stack[i] == ERHIRecordingContextScope::OutsideFrameResourceUpload)
				return true;
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
