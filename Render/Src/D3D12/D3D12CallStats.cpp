#include "D3D12/D3D12CallStats.h"
#include "RHI/RHI.h"

#include <atomic>

namespace Render
{
	namespace D3D12CallStats
	{
		static std::atomic<uint64_t> sExecuteCalls{0};
		static std::atomic<uint64_t> sExecuteLists{0};
		static std::atomic<uint64_t> sSignalCalls{0};
		static std::atomic<uint64_t> sPresentCalls{0};
		static std::atomic<uint64_t> sCreateCommittedCalls{0};
		static std::atomic<uint64_t> sMapCalls{0};
		static std::atomic<uint64_t> sUnmapCalls{0};
		static std::atomic<uint64_t> sFenceSetEventCalls{0};
		static std::atomic<uint64_t> sWaitForSingleObjectCalls{0};
		static std::atomic<uint64_t> sDirectFenceImmediateSignalCalls{0};
		static std::atomic<uint64_t> sDirectFenceDeferredReserveCalls{0};
		static std::atomic<uint64_t> sBarrierAdds{0};
		static std::atomic<uint64_t> sBarrierFlushCalls{0};
		static std::atomic<uint64_t> sBarrierFlushed{0};
		static std::atomic<uint64_t> sCopyBytes{0};
		static std::atomic<uint64_t> sUploadBytes{0};

		void IncExecuteCommandLists(uint32_t NumLists)
		{
			if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
				return;
			sExecuteCalls.fetch_add(1, std::memory_order_relaxed);
			sExecuteLists.fetch_add((uint64_t)NumLists, std::memory_order_relaxed);
		}

		void IncQueueSignal()
		{
			if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
				return;
			sSignalCalls.fetch_add(1, std::memory_order_relaxed);
		}

		void IncPresent()
		{
			if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
				return;
			sPresentCalls.fetch_add(1, std::memory_order_relaxed);
		}

		void IncCreateCommittedResource()
		{
			if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
				return;
			sCreateCommittedCalls.fetch_add(1, std::memory_order_relaxed);
		}

		void IncMap()
		{
			if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
				return;
			sMapCalls.fetch_add(1, std::memory_order_relaxed);
		}

		void IncUnmap()
		{
			if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
				return;
			sUnmapCalls.fetch_add(1, std::memory_order_relaxed);
		}

		void IncFenceSetEventOnCompletion()
		{
			if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
				return;
			sFenceSetEventCalls.fetch_add(1, std::memory_order_relaxed);
		}

		void IncWaitForSingleObject()
		{
			if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
				return;
			sWaitForSingleObjectCalls.fetch_add(1, std::memory_order_relaxed);
		}

		void IncDirectFenceImmediateSignal()
		{
			if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
				return;
			sDirectFenceImmediateSignalCalls.fetch_add(1, std::memory_order_relaxed);
		}

		void IncDirectFenceDeferredReserve()
		{
			if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
				return;
			sDirectFenceDeferredReserveCalls.fetch_add(1, std::memory_order_relaxed);
		}

		void AddResourceBarriers(uint32_t Count)
		{
			if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
				return;
			if (Count)
				sBarrierAdds.fetch_add((uint64_t)Count, std::memory_order_relaxed);
		}

		void FlushResourceBarriers(uint32_t FlushedCount)
		{
			if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
				return;
			sBarrierFlushCalls.fetch_add(1, std::memory_order_relaxed);
			if (FlushedCount)
				sBarrierFlushed.fetch_add((uint64_t)FlushedCount, std::memory_order_relaxed);
		}

		void AddCopyBytes(uint64_t Bytes)
		{
			if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
				return;
			if (Bytes)
				sCopyBytes.fetch_add(Bytes, std::memory_order_relaxed);
		}

		void AddUploadBytes(uint64_t Bytes)
		{
			if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
				return;
			if (Bytes)
				sUploadBytes.fetch_add(Bytes, std::memory_order_relaxed);
		}

		Snapshot SnapshotAndReset()
		{
			if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
				return {};
			Snapshot s;
			s.ExecuteCommandListsCalls = sExecuteCalls.exchange(0, std::memory_order_relaxed);
			s.ExecuteCommandListsLists = sExecuteLists.exchange(0, std::memory_order_relaxed);
			s.QueueSignalCalls = sSignalCalls.exchange(0, std::memory_order_relaxed);
			s.SwapchainPresentCalls = sPresentCalls.exchange(0, std::memory_order_relaxed);
			s.CreateCommittedResourceCalls = sCreateCommittedCalls.exchange(0, std::memory_order_relaxed);
			s.ResourceMapCalls = sMapCalls.exchange(0, std::memory_order_relaxed);
			s.ResourceUnmapCalls = sUnmapCalls.exchange(0, std::memory_order_relaxed);
			s.FenceSetEventOnCompletionCalls = sFenceSetEventCalls.exchange(0, std::memory_order_relaxed);
			s.WaitForSingleObjectCalls = sWaitForSingleObjectCalls.exchange(0, std::memory_order_relaxed);
			s.DirectFenceImmediateSignalCalls = sDirectFenceImmediateSignalCalls.exchange(0, std::memory_order_relaxed);
			s.DirectFenceDeferredReserveCalls = sDirectFenceDeferredReserveCalls.exchange(0, std::memory_order_relaxed);
			s.ResourceBarrierAdds = sBarrierAdds.exchange(0, std::memory_order_relaxed);
			s.ResourceBarrierFlushCalls = sBarrierFlushCalls.exchange(0, std::memory_order_relaxed);
			s.ResourceBarrierFlushed = sBarrierFlushed.exchange(0, std::memory_order_relaxed);
			s.CopyBytes = sCopyBytes.exchange(0, std::memory_order_relaxed);
			s.UploadBytes = sUploadBytes.exchange(0, std::memory_order_relaxed);
			return s;
		}
	}
}

