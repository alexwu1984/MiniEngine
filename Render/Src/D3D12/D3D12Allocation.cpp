#include "D3D12/D3D12Allocation.h"
#include "RHI/RHI.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12UploadPlacedBuddyPool.h"
#include "D3D12/D3D12DirectCommandListManager.h"
#include "D3D12/D3D12CreateStats.h"
#include "D3D12/D3D12UploadWCDiagnostics.h"
#include "core/commandline.h"

namespace RenderCore
{
	static_assert(CpuAllocatorPageSize == 4u * 1024u * 1024u, "UE 4.26 DefaultFastAllocator upload page size");
	static_assert(UE426_MIN_PLACED_BUFFER_SIZE == UE426_D3D_BUFFER_ALIGNMENT, "UE 4.26 D3D12Allocation.h placed-buffer min");

	std::vector<win32::com_ptr<ID3D12DescriptorHeap> > FD3D12ResourceAllocator::sm_DescriptorPool;

	FD3D12ResourceAllocator::FD3D12ResourceAllocator(std::weak_ptr<FD3D12Device> InParentDevice,
		D3D12_DESCRIPTOR_HEAP_TYPE InHeapType)
		: FD3D12DeviceChild(InParentDevice)
		, HeapType(InHeapType)
		, CurrentHeap(nullptr)
		, DescriptorSize(0)
	{
		DescriptorSize = GetParentDevice()->GetDevice()->GetDescriptorHandleIncrementSize(HeapType);
	}

	FD3D12ResourceAllocator::FDescriptorAllocation FD3D12ResourceAllocator::AllocateBlock(uint32_t Count)
	{
		Assert(Count > 0);

		// Find a heap with space in its free list.
		for (uint32_t heapIdx = 0; heapIdx < (uint32_t)Heaps.size(); ++heapIdx)
		{
			FHeapState& H = Heaps[heapIdx];
			for (uint32_t i = 0; i < (uint32_t)H.FreeList.size(); ++i)
			{
				FFreeBlock& B = H.FreeList[i];
				if (B.Count >= Count)
				{
					const uint32_t Offset = B.Offset;
					B.Offset += Count;
					B.Count -= Count;
					if (B.Count == 0)
						H.FreeList.erase(H.FreeList.begin() + i);

					FDescriptorAllocation Out;
					Out.Heap = H.Heap.get();
					Out.Count = Count;
					Out.HeapIndex = heapIdx;
					Out.Offset = Offset;
					Out.Cpu = Out.Heap->GetCPUDescriptorHandleForHeapStart();
					Out.Cpu.ptr += size_t(Offset) * size_t(DescriptorSize);
					return Out;
				}
			}
		}

		// No space: create a new heap and allocate from it.
		FHeapState NewHeap;
		NewHeap.Heap = {};
		{
			D3D12_DESCRIPTOR_HEAP_DESC Desc = {};
			Desc.NumDescriptors = sm_NumDescriptorsPerHeap;
			Desc.Type = HeapType;
			Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			Desc.NodeMask = 1;
			VERIFYD3DRESULT(GetParentDevice()->GetDevice()->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(NewHeap.Heap.get_init_ref())));
			if (D3D12RHI_ShouldEnableMemMon())
				sm_DescriptorPool.emplace_back(NewHeap.Heap);
		}
		NewHeap.FreeList.clear();
		if (sm_NumDescriptorsPerHeap > Count)
		{
			NewHeap.FreeList.push_back(FFreeBlock{ Count, sm_NumDescriptorsPerHeap - Count });
		}
		const uint32_t NewIndex = (uint32_t)Heaps.size();
		Heaps.push_back(std::move(NewHeap));

		FDescriptorAllocation Out;
		Out.HeapIndex = NewIndex;
		Out.Offset = 0;
		Out.Count = Count;
		Out.Heap = Heaps[NewIndex].Heap.get();
		Out.Cpu = Out.Heap->GetCPUDescriptorHandleForHeapStart();
		return Out;
	}

	void FD3D12ResourceAllocator::FreeBlock(const FDescriptorAllocation& Allocation)
	{
		if (!Allocation.IsValid())
			return;
		if (Allocation.HeapIndex >= Heaps.size())
			return;

		FHeapState& H = Heaps[Allocation.HeapIndex];
		if (H.Heap.get() != Allocation.Heap)
			return;

		FFreeBlock NewBlock{ Allocation.Offset, Allocation.Count };
		auto& L = H.FreeList;
		auto It = L.begin();
		while (It != L.end() && It->Offset < NewBlock.Offset)
			++It;
		It = L.insert(It, NewBlock);

		// Merge with previous
		if (It != L.begin())
		{
			auto Prev = It - 1;
			if (Prev->Offset + Prev->Count == It->Offset)
			{
				Prev->Count += It->Count;
				It = L.erase(It);
				It = Prev;
			}
		}
		// Merge with next
		if (It != L.end())
		{
			auto Next = It + 1;
			if (Next != L.end() && It->Offset + It->Count == Next->Offset)
			{
				It->Count += Next->Count;
				L.erase(Next);
			}
		}
	}

	void FD3D12ResourceAllocator::DestroyAll()
	{
		sm_DescriptorPool.clear();
	}

	ID3D12DescriptorHeap* FD3D12ResourceAllocator::RequestNewHeap(std::shared_ptr<FD3D12Device> InDevice,D3D12_DESCRIPTOR_HEAP_TYPE Type)
	{
		Assert(InDevice.get());
		D3D12_DESCRIPTOR_HEAP_DESC Desc = {};
		Desc.NumDescriptors = sm_NumDescriptorsPerHeap;
		Desc.Type = Type;
		Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		Desc.NodeMask = 1;

		win32::com_ptr<ID3D12DescriptorHeap> descriptorHeap;
		VERIFYD3DRESULT(InDevice->GetDevice()->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(descriptorHeap.get_init_ref())));
		if (D3D12RHI_ShouldEnableMemMon())
			sm_DescriptorPool.emplace_back(descriptorHeap);
		return descriptorHeap.get();
	}

	FD3D12FastAllocatorPage::FD3D12FastAllocatorPage(std::weak_ptr<FD3D12Device> ParentDevice, ID3D12Resource* InResource, 
												D3D12_RESOURCE_STATES InitialState, D3D12_RESOURCE_DESC const& InDesc, 
												D3D12_HEAP_TYPE InHeapType /*= D3D12_HEAP_TYPE_DEFAULT*/)
		:FD3D12Resource(ParentDevice,InResource,InitialState,InDesc,InHeapType)
	{
		void* mapped = Map();
		if (InHeapType == D3D12_HEAP_TYPE_UPLOAD)
		{
			D3D12UploadWCDiagnostics_OnUploadMap(L"FD3D12FastAllocatorPage", mapped, (uint64_t)InDesc.Width);
		}
	}

	void FD3D12FastAllocatorPage::BindPlacedBuddy(FD3D12UploadPlacedBuddyPool* Pool, uint32_t OffsetMinUnits, uint32_t Order, EFastAllocatorType OwnerAllocatorType)
	{
		PlacedBuddyPool = Pool;
		PlacedBuddyOffsetMinUnits = OffsetMinUnits;
		PlacedBuddyOrder = Order;
		PlacedBuddyOwnerAllocatorType = OwnerAllocatorType;
		bHasPlacedBuddyBinding = true;
	}

	FD3D12FastAllocatorPage::~FD3D12FastAllocatorPage()
	{
		Unmap();
		if (bHasPlacedBuddyBinding && PlacedBuddyPool)
		{
			if (auto Dev = GetParentDevice())
			{
				Dev->GetFastAllocator(PlacedBuddyOwnerAllocatorType).EnqueuePlacedBuddyFree(
					PlacedBuddyPool, PlacedBuddyOffsetMinUnits, PlacedBuddyOrder, GetFenceValue(), GetRetireQueueType());
			}
			PlacedBuddyPool = nullptr;
			bHasPlacedBuddyBinding = false;
		}
	}

	EFastAllocatorType FD3D12FastAllocator::ms_TypeCounter = DefaultFastAllocator;

	FD3D12FastAllocator::FD3D12FastAllocator(std::weak_ptr<FD3D12Device> InParentDevice)
		:FD3D12DeviceChild(InParentDevice)
	{
		AllocatorType = ms_TypeCounter;
		ms_TypeCounter = (EFastAllocatorType)(ms_TypeCounter + 1);
		Assert(ms_TypeCounter <= FastAllocator_Num);
	}

	void FD3D12FastAllocator::EnqueuePlacedBuddyFree(FD3D12UploadPlacedBuddyPool* Pool, uint32_t OffsetMinUnits, uint32_t Order, uint64_t FenceValue, ED3D12CommandQueueType QueueType)
	{
		if (!Pool)
			return;
		std::lock_guard<std::mutex> Lock(PlacedBuddyDeferredMutex);
		FPlacedBuddyPendingFree E{};
		E.Pool = Pool;
		E.OffsetMinUnits = OffsetMinUnits;
		E.Order = Order;
		E.FenceValue = FenceValue;
		E.QueueType = QueueType;
		PlacedBuddyDeferred.push_back(E);
	}

	void FD3D12FastAllocator::ProcessPlacedBuddyDeferredFrees()
	{
		auto Dev = GetParentDevice();
		if (!Dev)
			return;

		std::lock_guard<std::mutex> Lock(PlacedBuddyDeferredMutex);
		for (size_t i = 0; i < PlacedBuddyDeferred.size(); )
		{
			const FPlacedBuddyPendingFree& E = PlacedBuddyDeferred[i];
			FD3D12CommandListManager* Mgr = Dev->TryGetCommandListManager(E.QueueType);
			if (!Mgr)
			{
				++i;
				continue;
			}
			if (Mgr->GetFence().IsFenceComplete(E.FenceValue))
			{
				if (E.Pool)
					E.Pool->DeallocateBlock(E.OffsetMinUnits, E.Order);
				PlacedBuddyDeferred.erase(PlacedBuddyDeferred.begin() + (ptrdiff_t)i);
			}
			else
			{
				++i;
			}
		}
	}

	void FD3D12FastAllocator::DrainPlacedBuddyDeferredWithWait()
	{
		auto Dev = GetParentDevice();
		if (!Dev)
			return;
		for (int iter = 0; iter < 4096; ++iter)
		{
			ProcessPlacedBuddyDeferredFrees();
			uint64_t FenceVal = 0;
			ED3D12CommandQueueType Q = ED3D12CommandQueueType::Default;
			{
				std::lock_guard<std::mutex> Lock(PlacedBuddyDeferredMutex);
				if (PlacedBuddyDeferred.empty())
					return;
				FenceVal = PlacedBuddyDeferred.front().FenceValue;
				Q = PlacedBuddyDeferred.front().QueueType;
			}
			FD3D12CommandListManager* QMgr = Dev->TryGetCommandListManager(Q);
			if (!QMgr)
				return;
			QMgr->GetFence().WaitForFence(FenceVal);
		}
	}

	namespace
	{
		static int QueueTypeIndex(ED3D12CommandQueueType Q)
		{
			switch (Q)
			{
			case ED3D12CommandQueueType::Default: return 0;
			case ED3D12CommandQueueType::Copy:    return 1;
			case ED3D12CommandQueueType::Async:   return 2;
			default: return 0;
			}
		}

		static size_t MaxCachedStandardPages(EFastAllocatorType Type)
		{
			// These pools are a cache, not a requirement. Without a cap they can look like a leak
			// in Release builds where the CPU can outrun the GPU and peak allocations keep rising.
			switch (Type)
			{
			case EFastAllocatorType::DefaultFastAllocator: return 256;
			// Upload pool: UE 4.26 fast allocator uses 4MB pages; cap total cached standard pages (~80MB) when GPU lags.
			case EFastAllocatorType::UploadFastAllocator:  return 20;
			default: return 64;
			}
		}
	}

	uint64_t FD3D12AbstractRingBuffer::OldestOutstandingFenceValue() const
	{
		if (OutstandingAllocs.empty())
			return 0;
		return OutstandingAllocs.begin()->first;
	}

	void FD3D12AbstractRingBuffer::UpdateCompleted()
	{
		if (!Fence)
			return;

		const uint64_t LastCompletedFence = Fence->GetLastCompletedFenceFast();
		if (LastCompletedFence <= LastFence)
			return;

		LastFence = LastCompletedFence;
		for (auto It = OutstandingAllocs.begin(); It != OutstandingAllocs.end();)
		{
			// Allocations are keyed by the fence value that will be signaled on submit.
			// Once the fence is completed (>= key), the bytes are safe to reclaim.
			if (It->first <= LastCompletedFence)
			{
				const uint64_t Bytes = It->second;
				// Reclaim in FIFO order (fence values are monotonic).
				Head = (Size == 0) ? 0 : ((Head + Bytes) % Size);
				UsedSize = (Bytes > UsedSize) ? 0 : (UsedSize - Bytes);
				It = OutstandingAllocs.erase(It);
			}
			else
			{
				++It;
			}
		}
	}

	uint64_t FD3D12AbstractRingBuffer::Allocate(uint64_t Count)
	{
		UpdateCompleted();

		if (Count == 0 || Size == 0)
			return FailedReturnValue;
		if (Count > Size)
			return FailedReturnValue;

		// If we need to wrap to the beginning, allocate padding to end (UE-style) so the next alloc starts at 0.
		if (Tail + Count > Size)
		{
			// If head is at 0, we can't wrap yet.
			if (Head == 0)
			{
				D3D12MemMonAtomicAdd(D3D12CreateStats::TransientRing_AllocFailCount());
				return FailedReturnValue;
			}
			const uint64_t Padding = Size - Tail;
			// Padding is purely to advance Tail; still must be fenced because it occupies bytes.
			const uint64_t FenceKey = Fence ? Fence->GetCurrentFence() : 0;
			OutstandingAllocs[FenceKey] += Padding;
			UsedSize += Padding;
			Tail = 0;

			D3D12MemMonAtomicAdd(D3D12CreateStats::TransientRing_WrapCount());
			D3D12MemMonAtomicAdd(D3D12CreateStats::TransientRing_WrapBytes(), Padding);
		}

		// Space check: ring is full when UsedSize + Count > Size.
		if (UsedSize + Count > Size)
		{
			D3D12MemMonAtomicAdd(D3D12CreateStats::TransientRing_AllocFailCount());
			return FailedReturnValue;
		}

		// If the allocation would overlap Head (when wrapped), fail.
		if (Tail >= Head)
		{
			// Region [Tail, Tail+Count) is valid as long as it doesn't run past Size (handled above)
			// and doesn't overlap head when head is between Tail..Size.
			// When Tail>=Head, the free region is [Tail..Size) plus [0..Head).
			// Since we ensured Tail+Count<=Size, we're good.
		}
		else
		{
			// Tail < Head: free region is [Tail..Head). Must fit.
			if (Tail + Count > Head)
			{
				D3D12MemMonAtomicAdd(D3D12CreateStats::TransientRing_AllocFailCount());
				return FailedReturnValue;
			}
		}

		const uint64_t ReturnValue = Tail;
		Tail += Count;
		UsedSize += Count;

		const uint64_t FenceKey = Fence ? Fence->GetCurrentFence() : 0;
		OutstandingAllocs[FenceKey] += Count;

		return ReturnValue;
	}

	uint64_t FD3D12AbstractRingBuffer::AllocateOrWait(uint64_t Count)
	{
		if (!Fence)
			return Allocate(Count);

		// Can't satisfy allocations larger than the ring.
		if (Count > Size)
			return FailedReturnValue;

		for (;;)
		{
			// Always try a non-blocking allocation first. At startup (before any Signal),
			// the ring is empty and this should succeed; only the "wait for wrap" path
			// needs a valid signaled fence.
			const uint64_t Off = Allocate(Count);
			if (Off != FailedReturnValue)
				return Off;

			// If we haven't signaled anything yet (startup / before first Present), waiting would deadlock.
			// Let the caller fall back to a different allocation path.
			if (Fence->GetLastSignaledFence() == 0)
				return FailedReturnValue;

			const uint64_t Oldest = OldestOutstandingFenceValue();
			if (Oldest == 0)
				return FailedReturnValue;

			// Ring is full: wait for the oldest outstanding allocation to complete (UE-style).
			D3D12MemMonAtomicAdd(D3D12CreateStats::TransientRing_WaitCount());
			D3D12MemMonAtomicAdd(D3D12CreateStats::TransientRing_WaitBytes(), Count);
			Fence->WaitForFence(Oldest);
			UpdateCompleted();
		}
	}

	FD3D12TransientUploadRing::FD3D12TransientUploadRing(std::weak_ptr<FD3D12Adapter> InParentAdapter)
		: FD3D12AdapterChild(InParentAdapter)
		, Ring(0)
	{
	}

	FD3D12TransientUploadRing::~FD3D12TransientUploadRing()
	{
		Destroy();
	}

	bool FD3D12TransientUploadRing::Initialize(uint64_t BufferBytes)
	{
		if (UploadResource)
			return true;

		if (BufferBytes == 0)
			return false;

		auto Adapter = GetParentAdapter();
		if (!Adapter)
			return false;

		SizeBytes = BufferBytes;
		Ring.Reset(SizeBytes);
		Ring.SetFence(&Adapter->GetFrameFence());

		D3D12_HEAP_PROPERTIES HeapProps = {};
		HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProps.CreationNodeMask = 1;
		HeapProps.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC Desc = {};
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		Desc.Alignment = 0;
		Desc.Width = SizeBytes;
		Desc.Height = 1;
		Desc.DepthOrArraySize = 1;
		Desc.MipLevels = 1;
		Desc.Format = DXGI_FORMAT_UNKNOWN;
		Desc.SampleDesc.Count = 1;
		Desc.SampleDesc.Quality = 0;
		Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		Desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		win32::com_ptr<ID3D12Resource> R;
		VERIFYD3DRESULT(Adapter->GetD3DDevice()->CreateCommittedResource(
			&HeapProps, D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(R.get_init_ref())));
		R->SetName(L"TransientUploadRing");

		void* Cpu = nullptr;
		D3D12_RANGE range = { 0, 0 };
		VERIFYD3DRESULT(R->Map(0, &range, &Cpu));

		UploadResource = std::move(R);
		MappedBase = Cpu;

		if (D3D12RHI_ShouldEnableMemMon())
		{
			D3D12UploadWCDiagnostics_OnUploadMap(L"TransientUploadRing", MappedBase, SizeBytes);
		}

		// Optional: pre-touch the ring to commit WC pages upfront.
		// This avoids the appearance of a "leak" where VMemPrivate WC bytes climbs for many seconds
		// as the CPU touches new upload pages over time.
		// Enable via command line: d3d12_upload_ring_pretouch=1
		{
			int32_t Pretouch = 0;
			core::CommandLine::Get().GetInteger("d3d12_upload_ring_pretouch", Pretouch);
			if (Pretouch != 0 && MappedBase && SizeBytes)
			{
				// Touch one byte per 4KB page to force commit.
				// Using 64KB steps would only touch 1 page out of 16 on 4KB-page systems.
				volatile uint8_t* p = (volatile uint8_t*)MappedBase;
				const uint64_t Step = 4ull * 1024ull;
				for (uint64_t off = 0; off < SizeBytes; off += Step)
				{
					p[off] = 0;
				}
				// Touch the last byte as well.
				p[SizeBytes - 1] = 0;
			}
		}

		return true;
	}

	void FD3D12TransientUploadRing::Destroy()
	{
		if (UploadResource)
		{
			UploadResource->Unmap(0, nullptr);
			UploadResource = {};
		}
		MappedBase = nullptr;
		SizeBytes = 0;
		Ring.Reset(0);
	}

	FAllocation FD3D12TransientUploadRing::Allocate(uint64_t Bytes, uint64_t Alignment)
	{
		FAllocation Out = {};
		if (!UploadResource || !MappedBase || SizeBytes == 0 || Bytes == 0)
			return Out;

		const uint64_t AlignedBytes = AlignUp(Bytes, Alignment);
		if (AlignedBytes > SizeBytes)
			return Out;
		const uint64_t Off = Ring.AllocateOrWait(AlignedBytes);
		if (Off == FD3D12AbstractRingBuffer::FailedReturnValue)
			return Out;

		Out.Resource = nullptr; // this allocation is backed by UploadResource, not a fast allocator page
		Out.Offset = (size_t)Off;
		Out.CPU = (uint8_t*)MappedBase + Off;
		Out.GpuAddress = UploadResource->GetGPUVirtualAddress() + Off;
		Out.D3D12Resource = UploadResource.get();
		return Out;
	}

	FD3D12FastAllocatorPage* FD3D12FastAllocator::RequestPage()
	{
		ProcessPlacedBuddyDeferredFrees();

		FD3D12FastAllocatorPage* Page = nullptr;

		// Promote completed retired pages into a ready queue (O(1)).
		// Retired pages are monotonic in fence value per-queue, so checking only the front is enough.
		for (int q = 0; q < 3; ++q)
		{
			while (!RetiredPages[q].empty())
			{
				FD3D12FastAllocatorPage* Candidate = RetiredPages[q].front();
				auto& Mgr = GetParentDevice()->GetCommandListManager(Candidate->GetRetireQueueType());
				if (!Mgr.GetFence().IsFenceComplete(Candidate->GetFenceValue()))
					break;
				ReadyPages.push(Candidate);
				RetiredPages[q].pop();
			}
		}

		if (!ReadyPages.empty())
		{
			Page = ReadyPages.front();
			ReadyPages.pop();
			D3D12MemMonAtomicAdd(D3D12CreateStats::LinearPage_ReuseFromReadyCount());
		}

		if (Page == nullptr)
		{
			// Prevent unbounded growth when the GPU falls behind.
			// If we already created "enough" standard pages and none are ready, wait for the
			// oldest retired page to complete and reuse it instead of creating more.
			const size_t MaxPages = MaxCachedStandardPages(AllocatorType);
			const bool bHasAnyRetired = (!RetiredPages[0].empty() || !RetiredPages[1].empty() || !RetiredPages[2].empty());
			if (OwnedStandardPages.size() >= MaxPages && bHasAnyRetired)
			{
				// Pick a queue to wait on (prefer Default queue).
				int PickQ = !RetiredPages[0].empty() ? 0 : (!RetiredPages[1].empty() ? 1 : 2);
				FD3D12FastAllocatorPage* Oldest = RetiredPages[PickQ].front();
				RetiredPages[PickQ].pop();

				auto& Mgr = GetParentDevice()->GetCommandListManager(Oldest->GetRetireQueueType());
				D3D12MemMonAtomicAdd(D3D12CreateStats::LinearPage_FenceWaitReuseCount());
				Mgr.GetFence().WaitForFence(Oldest->GetFenceValue());
				ReadyPages.push(Oldest);

				Page = ReadyPages.front();
				ReadyPages.pop();
				D3D12MemMonAtomicAdd(D3D12CreateStats::LinearPage_ReuseFromReadyCount());
			}
			else
			{
				Page = CreateNewPage();
			}
		}
		
		Assert(Page != nullptr);
		return Page;
	}

	void FD3D12FastAllocator::DiscardStandardPages(uint64_t FenceID, ED3D12CommandQueueType QueueType, const std::vector<FD3D12FastAllocatorPage*>& Pages)
	{
		if (!Pages.empty())
		{
			D3D12MemMonAtomicAdd(D3D12CreateStats::LinearPage_DiscardStandardPageCount(), (uint64_t)Pages.size());
		}
		for (auto Iter = Pages.begin(); Iter != Pages.end(); ++Iter)
		{
			(*Iter)->SetFenceValue(FenceID);
			(*Iter)->SetRetireQueueType(QueueType);
			RetiredPages[QueueTypeIndex(QueueType)].push(*Iter);
		}

		// Trim standard page cache when GPU has caught up.
		// Only release pages that are back in the ready queue (i.e. not in flight).
		const size_t MaxPages = MaxCachedStandardPages(AllocatorType);
		while (OwnedStandardPages.size() > MaxPages && !ReadyPages.empty())
		{
			FD3D12FastAllocatorPage* Candidate = ReadyPages.front();
			ReadyPages.pop();

			// Candidate is already "ready", so its fence must be complete. Evict it from ownership.
			for (size_t i = 0; i < OwnedStandardPages.size(); ++i)
			{
				if (OwnedStandardPages[i] == Candidate)
				{
					OwnedStandardPages[i] = OwnedStandardPages.back();
					OwnedStandardPages.pop_back();
					break;
				}
			}
			D3D12MemMonAtomicAdd(D3D12CreateStats::LinearPage_StandardCacheReleaseCount());
			Candidate->Release();
		}
	}

	void FD3D12FastAllocator::DiscardLargePages(uint64_t FenceID, ED3D12CommandQueueType QueueType, const std::vector<FD3D12FastAllocatorPage*>& Pages)
	{
		// Large pages are one-off allocations. Don't pool them: retire and delete after the fence passes.
		// This matches the behavior in DirectX-Graphics-Samples MiniEngine and avoids unbounded WC growth.
		while (!LargePageDeletionQueue.empty())
		{
			const FLargePageDelete& Front = LargePageDeletionQueue.front();
			auto& Mgr = GetParentDevice()->GetCommandListManager(Front.QueueType);
			if (!Mgr.GetFence().IsFenceComplete(Front.FenceValue))
				break;

			if (Front.Page)
			{
				D3D12MemMonAtomicAdd(D3D12CreateStats::LinearPage_LargePageDestroyedCount());
				Front.Page->Release();
			}
			LargePageDeletionQueue.pop();
		}

		for (auto Iter = Pages.begin(); Iter != Pages.end(); ++Iter)
		{
			FD3D12FastAllocatorPage* P = *Iter;
			if (!P)
				continue;
			P->SetFenceValue(FenceID);
			P->SetRetireQueueType(QueueType);
			// Optional but helps diagnostics and reduces mapped CPU VA footprint.
			P->Unmap();
			LargePageDeletionQueue.push(FLargePageDelete{ FenceID, QueueType, P });
		}
	}

	FD3D12FastAllocatorPage* FD3D12FastAllocator::CreateNewPage(size_t PageSize /*= 0*/)
	{
		D3D12_HEAP_PROPERTIES HeapProps;
		HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProps.CreationNodeMask = 1;
		HeapProps.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC ResourceDesc;
		ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		ResourceDesc.Alignment = 0;
		ResourceDesc.Height = 1;
		//uboResourceDesc.Width = (sizeof(m_uboVS) + 255) & ~255;
		ResourceDesc.DepthOrArraySize = 1;
		ResourceDesc.MipLevels = 1;
		ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		ResourceDesc.SampleDesc.Count = 1;
		ResourceDesc.SampleDesc.Quality = 0;
		ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_RESOURCE_STATES DefaultUsage;
		if (AllocatorType == EFastAllocatorType::DefaultFastAllocator)
		{
			HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
			ResourceDesc.Width = PageSize == 0 ? GpuAllocatorPageSize : PageSize;
			ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			DefaultUsage = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}
		else
		{
			HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
			ResourceDesc.Width = PageSize == 0 ? CpuAllocatorPageSize : PageSize;
			ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			DefaultUsage = D3D12_RESOURCE_STATE_GENERIC_READ;
		}

		win32::com_ptr<ID3D12Resource> pBuffer;
		FD3D12UploadPlacedBuddyPool* BuddyPool = nullptr;
		uint32_t BuddyOff = 0, BuddyOrd = 0;
		bool bPlacedBuddyPage = false;

		// Stats (same counters whether committed or placed-buddy suballoc).
		{
			const uint64_t Bytes = (uint64_t)ResourceDesc.Width;
			if (HeapProps.Type == D3D12_HEAP_TYPE_DEFAULT)
			{
				D3D12MemMonAtomicAdd(D3D12CreateStats::LinearPage_CreateCount_Default());
				D3D12MemMonAtomicAdd(D3D12CreateStats::LinearPage_CreateBytes_Default(), Bytes);
			}
			else if (HeapProps.Type == D3D12_HEAP_TYPE_UPLOAD)
			{
				D3D12MemMonAtomicAdd(D3D12CreateStats::LinearPage_CreateCount_Upload());
				D3D12MemMonAtomicAdd(D3D12CreateStats::LinearPage_CreateBytes_Upload(), Bytes);
				if (PageSize != 0)
				{
					D3D12MemMonAtomicAdd(D3D12CreateStats::LinearPage_UploadLargeCreateCount());
					D3D12MemMonAtomicAdd(D3D12CreateStats::LinearPage_UploadLargeCreateBytes(), Bytes);
				}
			}
		}

		if (HeapProps.Type == D3D12_HEAP_TYPE_UPLOAD && PageSize == 0 && AllocatorType == EFastAllocatorType::UploadFastAllocator)
		{
			BuddyPool = GetParentDevice()->GetUploadPlacedBuddyPool();
			uint64_t GpuVA = 0;
			void* Cpu = nullptr;
			if (BuddyPool)
			{
				// If the pool is temporarily out of space (GPU behind, frees not processed yet),
				// wait for the oldest deferred free and retry. This prevents unbounded WC region growth.
				if (!BuddyPool->TryAllocatePlacedUploadPage(ResourceDesc.Width, pBuffer, GpuVA, Cpu, BuddyOff, BuddyOrd))
				{
					DrainPlacedBuddyDeferredWithWait();
					(void)BuddyPool->TryAllocatePlacedUploadPage(ResourceDesc.Width, pBuffer, GpuVA, Cpu, BuddyOff, BuddyOrd);
				}
				if (pBuffer)
				{
					(void)GpuVA;
					(void)Cpu;
					bPlacedBuddyPage = true;
				}
			}
		}

		if (!bPlacedBuddyPage)
		{
			VERIFYD3DRESULT(GetParentDevice()->GetDevice()->CreateCommittedResource(
				&HeapProps,
				D3D12_HEAP_FLAG_NONE,
				&ResourceDesc,
				DefaultUsage,
				nullptr,
				IID_PPV_ARGS(&pBuffer)));
			pBuffer->SetName(L"FD3D12FastAllocatorPage");
		}

		FD3D12FastAllocatorPage* AllocationPage = new FD3D12FastAllocatorPage(GetParentDevice(), pBuffer.get(), DefaultUsage, ResourceDesc, HeapProps.Type);
		if (bPlacedBuddyPage && BuddyPool)
			AllocationPage->BindPlacedBuddy(BuddyPool, BuddyOff, BuddyOrd, AllocatorType);
		AllocationPage->AddRef();
		// Own one ref for the lifetime of this manager (availability is tracked via ready/retired queues).
		if (HeapProps.Type == D3D12_HEAP_TYPE_UPLOAD || HeapProps.Type == D3D12_HEAP_TYPE_DEFAULT)
		{
			if (PageSize == 0)
			{
				// Standard page (not a per-allocation large page). Large pages are tracked separately.
				OwnedStandardPages.push_back(AllocationPage);
			}
		}
		return AllocationPage;
	}

	void FD3D12FastAllocator::Destroy()
	{
		while (!LargePageDeletionQueue.empty())
		{
			if (LargePageDeletionQueue.front().Page)
				LargePageDeletionQueue.front().Page->Release();
			LargePageDeletionQueue.pop();
		}
		for (FD3D12FastAllocatorPage* P : OwnedStandardPages)
			P->Release();
		OwnedStandardPages.clear();
		while (!ReadyPages.empty())
			ReadyPages.pop();
		// Pages still waiting in retired queues share refs released via StandardPagePool above; drop stale pointers.
		for (int q = 0; q < 3; ++q)
		{
			while (!RetiredPages[q].empty())
				RetiredPages[q].pop();
		}
		DrainPlacedBuddyDeferredWithWait();
	}

	EFastAllocatorType FD3D12FastAllocator::GetAllocatorType() const
	{
		return AllocatorType;
	}

	FD3D12LinearAllocator::FD3D12LinearAllocator(EFastAllocatorType Type, std::weak_ptr<FD3D12Device> ParentDevice)
		:FD3D12DeviceChild(ParentDevice)
		,m_AllocatorType(Type)
		, m_CurrentPage(nullptr)
		, m_CurrentOffset(0)
	{
		Assert(Type > EFastAllocatorType::InvalidFastAllocator && Type < EFastAllocatorType::FastAllocator_Num);
		m_PageSize = (Type == EFastAllocatorType::DefaultFastAllocator ? GpuAllocatorPageSize : CpuAllocatorPageSize);
	}

	FAllocation FD3D12LinearAllocator::Allocate(size_t SizeInBytes, size_t Alignment /*= DEFAULT_ALIGN*/)
	{
		const size_t AlignmentMask = Alignment - 1;
		Assert((AlignmentMask & Alignment) == 0);
		const size_t AlignedSize = math::AlignUpWithMask(SizeInBytes, AlignmentMask);

		// UE-style: for transient UPLOAD allocations, prefer a bounded fence-aware ring.
		// This prevents unbounded WC commit growth caused by linear allocation patterns touching new pages forever.
		if (m_AllocatorType == EFastAllocatorType::UploadFastAllocator)
		{
			auto Dev = GetParentDevice();
			auto Adapter = Dev ? Dev->GetParentAdapter() : nullptr;
			if (Adapter)
			{
				FD3D12TransientUploadRing& Ring = Adapter->GetTransientUploadRing();
				FAllocation a = Ring.Allocate((uint64_t)AlignedSize, (uint64_t)Alignment);
				if (a.CPU && a.GpuAddress != 0 && a.D3D12Resource)
					return a;
			}
		}

		if (AlignedSize > m_PageSize)
			return AllocateLargePage(AlignedSize);

		m_CurrentOffset = math::AlignUp(m_CurrentOffset, Alignment);
		if (m_CurrentOffset + AlignedSize > m_PageSize)
		{
			Assert(m_CurrentPage != nullptr);
			// make sure current page is in UsingPages
			m_StandardPages.push_back(m_CurrentPage);
			m_CurrentPage = nullptr;
		}

		if (m_CurrentPage == nullptr)
		{
			m_CurrentPage = GetParentDevice()->GetFastAllocator(m_AllocatorType).RequestPage();
			Assert(m_CurrentPage != nullptr);
			m_CurrentOffset = 0;
		}

		Assert(m_CurrentPage != nullptr);
		FAllocation allocation;
		allocation.Resource = m_CurrentPage;
		allocation.D3D12Resource = m_CurrentPage->GetResource();
		allocation.Offset = m_CurrentOffset;
		allocation.CPU = (uint8_t*)m_CurrentPage->GetResourceBaseAddress() + m_CurrentOffset;
		allocation.GpuAddress = m_CurrentPage->GetGPUVirtualAddress() + m_CurrentOffset;
		m_CurrentOffset += AlignedSize;
		return allocation;
	}

	void FD3D12LinearAllocator::CleanupUsedPages(uint64_t FenceID, ED3D12CommandQueueType QueueType)
	{
		if (m_CurrentPage != nullptr)
		{
			m_StandardPages.push_back(m_CurrentPage);
			m_CurrentPage = nullptr;
			m_CurrentOffset = 0;
		}

		GetParentDevice()->GetFastAllocator(m_AllocatorType).DiscardStandardPages(FenceID, QueueType, m_StandardPages);
		m_StandardPages.clear();

		GetParentDevice()->GetFastAllocator(m_AllocatorType).DiscardLargePages(FenceID, QueueType, m_LargePages);
		m_LargePages.clear();
	}

	FAllocation FD3D12LinearAllocator::AllocateLargePage(size_t SizeInBytes)
	{
		// Diagnostics are kept out of allocator core logic.
		if (m_AllocatorType == EFastAllocatorType::UploadFastAllocator)
			D3D12UploadWCDiagnostics_OnAllocateLargePage(L"EFastAllocator_Upload", SizeInBytes);

		FD3D12FastAllocatorPage* Page = GetParentDevice()->GetFastAllocator(m_AllocatorType).CreateNewPage(SizeInBytes);
		m_LargePages.push_back(Page);

		FAllocation allocation;
		allocation.Resource = Page;
		allocation.D3D12Resource = Page->GetResource();
		allocation.Offset = 0;
		allocation.CPU = (uint8_t*)Page->GetResourceBaseAddress();
		allocation.GpuAddress = Page->GetGPUVirtualAddress();
		return allocation;
	}

}