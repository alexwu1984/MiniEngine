#pragma once
#include "core/inc.h"

namespace Render
{
	// Lightweight per-second counters to correlate driver-side memory growth
	// with high-frequency D3D12/DXGI calls made by the engine.
	namespace D3D12CallStats
	{
		struct Snapshot
		{
			uint64_t ExecuteCommandListsCalls = 0;
			uint64_t ExecuteCommandListsLists = 0;
			uint64_t QueueSignalCalls = 0;
			uint64_t SwapchainPresentCalls = 0;
			uint64_t CreateCommittedResourceCalls = 0;
			uint64_t ResourceMapCalls = 0;
			uint64_t ResourceUnmapCalls = 0;
			uint64_t FenceSetEventOnCompletionCalls = 0;
			uint64_t WaitForSingleObjectCalls = 0;
			uint64_t DirectFenceImmediateSignalCalls = 0;
			uint64_t DirectFenceDeferredReserveCalls = 0;
			uint64_t ResourceBarrierAdds = 0;
			uint64_t ResourceBarrierFlushCalls = 0;
			uint64_t ResourceBarrierFlushed = 0;
			uint64_t CopyBytes = 0;
			uint64_t UploadBytes = 0;
		};

		void IncExecuteCommandLists(uint32_t NumLists);
		void IncQueueSignal();
		void IncPresent();
		void IncCreateCommittedResource();
		void IncMap();
		void IncUnmap();
		void IncFenceSetEventOnCompletion();
		void IncWaitForSingleObject();
		void IncDirectFenceImmediateSignal();
		void IncDirectFenceDeferredReserve();
		void AddResourceBarriers(uint32_t Count);
		void FlushResourceBarriers(uint32_t FlushedCount);
		void AddCopyBytes(uint64_t Bytes);
		void AddUploadBytes(uint64_t Bytes);

		// Grabs a snapshot and resets counters to 0.
		Snapshot SnapshotAndReset();
	}
}

