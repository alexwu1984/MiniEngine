#include "D3D12/D3D12Allocation.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12DirectCommandListManager.h"
#include "D3D12/D3D12CreateStats.h"
#include "D3D12/D3D12UploadWCDiagnostics.h"
#include "D3D12/D3D12CallStats.h"

#include "core/logger.h"

namespace RenderCore
{
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
			// Keep static tracking for global teardown/diagnostics.
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
		sm_DescriptorPool.emplace_back(descriptorHeap);
		return descriptorHeap.get();
	}

	LinearAllocationPage::LinearAllocationPage(std::weak_ptr<FD3D12Device> ParentDevice, ID3D12Resource* InResource, 
												D3D12_RESOURCE_STATES InitialState, D3D12_RESOURCE_DESC const& InDesc, 
												D3D12_HEAP_TYPE InHeapType /*= D3D12_HEAP_TYPE_DEFAULT*/)
		:FD3D12Resource(ParentDevice,InResource,InitialState,InDesc,InHeapType)
	{
		void* mapped = Map();
		if (InHeapType == D3D12_HEAP_TYPE_UPLOAD)
		{
			D3D12UploadWCDiagnostics_OnUploadMap(L"LinearAllocationPage", mapped, (uint64_t)InDesc.Width);
		}
	}

	LinearAllocationPage::~LinearAllocationPage()
	{
		Unmap();
	}

	ELinearAllocatorType LinearAllocationPageManager::ms_TypeCounter = GpuExclusive;

	LinearAllocationPageManager::LinearAllocationPageManager(std::weak_ptr<FD3D12Device> InParentDevice)
		:FD3D12DeviceChild(InParentDevice)
	{
		AllocatorType = ms_TypeCounter;
		ms_TypeCounter = (ELinearAllocatorType)(ms_TypeCounter + 1);
		Assert(ms_TypeCounter <= NumAllocatorTypes);
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

		static size_t MaxCachedStandardPages(ELinearAllocatorType Type)
		{
			// These pools are a cache, not a requirement. Without a cap they can look like a leak
			// in Release builds where the CPU can outrun the GPU and peak allocations keep rising.
			switch (Type)
			{
			case ELinearAllocatorType::GpuExclusive: return 256;
			// CpuWritable pages are typically 2MB. Keep a small cap and block when GPU falls behind,
			// otherwise WriteCombine upload pages can grow unbounded in Release.
			case ELinearAllocatorType::CpuWritable:  return 32;
			default: return 64;
			}
		}
	}

	LinearAllocationPage* LinearAllocationPageManager::RequestPage()
	{
		LinearAllocationPage* Page = nullptr;

		// Promote completed retired pages into a ready queue (O(1)).
		// Retired pages are monotonic in fence value per-queue, so checking only the front is enough.
		for (int q = 0; q < 3; ++q)
		{
			while (!RetiredPages[q].empty())
			{
				LinearAllocationPage* Candidate = RetiredPages[q].front();
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
			D3D12CreateStats::LinearPage_ReuseFromReadyCount().fetch_add(1, std::memory_order_relaxed);
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
				LinearAllocationPage* Oldest = RetiredPages[PickQ].front();
				RetiredPages[PickQ].pop();

				auto& Mgr = GetParentDevice()->GetCommandListManager(Oldest->GetRetireQueueType());
				// Evidence log (throttled): we are blocking the CPU to avoid unbounded upload page growth.
				{
					static ULONGLONG sLastLog = 0;
					const ULONGLONG now = ::GetTickCount64();
					if (now - sLastLog > 1000)
					{
						sLastLog = now;
						core::LOG(core::log_inf,
							L"[D3D12] LinearPageManager(%d) waiting for fence=%llu (owned=%zu ready=%zu retired=%zu/%zu/%zu)",
							(int)AllocatorType,
							(unsigned long long)Oldest->GetFenceValue(),
							OwnedStandardPages.size(),
							ReadyPages.size(),
							RetiredPages[0].size(), RetiredPages[1].size(), RetiredPages[2].size());
					}
				}
				D3D12CreateStats::LinearPage_FenceWaitReuseCount().fetch_add(1, std::memory_order_relaxed);
				Mgr.GetFence().WaitForFence(Oldest->GetFenceValue());
				ReadyPages.push(Oldest);

				Page = ReadyPages.front();
				ReadyPages.pop();
				D3D12CreateStats::LinearPage_ReuseFromReadyCount().fetch_add(1, std::memory_order_relaxed);
			}
			else
			{
			Page = CreateNewPage();
			}
		}
		
		Assert(Page != nullptr);
		return Page;
	}

	void LinearAllocationPageManager::DiscardStandardPages(uint64_t FenceID, ED3D12CommandQueueType QueueType, const std::vector<LinearAllocationPage*>& Pages)
	{
		if (!Pages.empty())
		{
			D3D12CreateStats::LinearPage_DiscardStandardPageCount().fetch_add(
				(uint64_t)Pages.size(), std::memory_order_relaxed);
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
			LinearAllocationPage* Candidate = ReadyPages.front();
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
			D3D12CreateStats::LinearPage_StandardCacheReleaseCount().fetch_add(1, std::memory_order_relaxed);
			Candidate->Release();
		}
	}

	void LinearAllocationPageManager::DiscardLargePages(uint64_t FenceID, ED3D12CommandQueueType QueueType, const std::vector<LinearAllocationPage*>& Pages)
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
				D3D12CreateStats::LinearPage_LargePageDestroyedCount().fetch_add(1, std::memory_order_relaxed);
				Front.Page->Release();
			}
			LargePageDeletionQueue.pop();
		}

		for (auto Iter = Pages.begin(); Iter != Pages.end(); ++Iter)
		{
			LinearAllocationPage* P = *Iter;
			if (!P)
				continue;
			P->SetFenceValue(FenceID);
			P->SetRetireQueueType(QueueType);
			// Optional but helps diagnostics and reduces mapped CPU VA footprint.
			P->Unmap();
			LargePageDeletionQueue.push(FLargePageDelete{ FenceID, QueueType, P });
		}
	}

	LinearAllocationPage* LinearAllocationPageManager::CreateNewPage(size_t PageSize /*= 0*/)
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
		if (AllocatorType == ELinearAllocatorType::GpuExclusive)
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
		// This CreateCommittedResource bypasses FD3D12Resource tracking; keep a direct counter for leak/churn triage.
		{
			const uint64_t Bytes = (uint64_t)ResourceDesc.Width;
			if (HeapProps.Type == D3D12_HEAP_TYPE_DEFAULT)
			{
				D3D12CreateStats::LinearPage_CreateCount_Default().fetch_add(1, std::memory_order_relaxed);
				D3D12CreateStats::LinearPage_CreateBytes_Default().fetch_add(Bytes, std::memory_order_relaxed);
			}
			else if (HeapProps.Type == D3D12_HEAP_TYPE_UPLOAD)
			{
				D3D12CreateStats::LinearPage_CreateCount_Upload().fetch_add(1, std::memory_order_relaxed);
				D3D12CreateStats::LinearPage_CreateBytes_Upload().fetch_add(Bytes, std::memory_order_relaxed);
				if (PageSize != 0)
				{
					D3D12CreateStats::LinearPage_UploadLargeCreateCount().fetch_add(1, std::memory_order_relaxed);
					D3D12CreateStats::LinearPage_UploadLargeCreateBytes().fetch_add(Bytes, std::memory_order_relaxed);
				}
			}
		}
		VERIFYD3DRESULT(GetParentDevice()->GetDevice()->CreateCommittedResource(
			&HeapProps,
			D3D12_HEAP_FLAG_NONE,
			&ResourceDesc,
			DefaultUsage,
			nullptr,
			IID_PPV_ARGS(&pBuffer)));

		pBuffer->SetName(L"LinearAllocatorPage");

		LinearAllocationPage * AllocationPage =  new LinearAllocationPage(GetParentDevice(),pBuffer.get(), DefaultUsage, ResourceDesc, HeapProps.Type);
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

	void LinearAllocationPageManager::Destroy()
	{
		while (!LargePageDeletionQueue.empty())
		{
			if (LargePageDeletionQueue.front().Page)
				LargePageDeletionQueue.front().Page->Release();
			LargePageDeletionQueue.pop();
		}
		for (LinearAllocationPage* P : OwnedStandardPages)
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
	}

	ELinearAllocatorType LinearAllocationPageManager::GetAllocatorType() const
	{
		return AllocatorType;
	}

	LinearAllocator::LinearAllocator(ELinearAllocatorType Type, std::weak_ptr<FD3D12Device> ParentDevice)
		:FD3D12DeviceChild(ParentDevice)
		,m_AllocatorType(Type)
		, m_CurrentPage(nullptr)
		, m_CurrentOffset(0)
	{
		Assert(Type > ELinearAllocatorType::InvalidAllocator && Type < ELinearAllocatorType::NumAllocatorTypes);
		m_PageSize = (Type == ELinearAllocatorType::GpuExclusive ? GpuAllocatorPageSize : CpuAllocatorPageSize);
	}

	FAllocation LinearAllocator::Allocate(size_t SizeInBytes, size_t Alignment /*= DEFAULT_ALIGN*/)
	{
		const size_t AlignmentMask = Alignment - 1;
		Assert((AlignmentMask & Alignment) == 0);
		const size_t AlignedSize = math::AlignUpWithMask(SizeInBytes, AlignmentMask);

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
			m_CurrentPage = GetParentDevice()->GetLinearPageManager(m_AllocatorType).RequestPage();
			Assert(m_CurrentPage != nullptr);
			m_CurrentOffset = 0;
		}

		Assert(m_CurrentPage != nullptr);
		FAllocation allocation;
		allocation.Resource = m_CurrentPage;
		//allocation.D3d12Resource = m_CurrentPage->GetResource();
		allocation.Offset = m_CurrentOffset;
		allocation.CPU = (uint8_t*)m_CurrentPage->GetResourceBaseAddress() + m_CurrentOffset;
		allocation.GpuAddress = m_CurrentPage->GetGPUVirtualAddress() + m_CurrentOffset;
		m_CurrentOffset += AlignedSize;
		return allocation;
	}

	void LinearAllocator::CleanupUsedPages(uint64_t FenceID, ED3D12CommandQueueType QueueType)
	{
		if (m_CurrentPage != nullptr)
		{
			m_StandardPages.push_back(m_CurrentPage);
			m_CurrentPage = nullptr;
			m_CurrentOffset = 0;
		}

		GetParentDevice()->GetLinearPageManager(m_AllocatorType).DiscardStandardPages(FenceID, QueueType, m_StandardPages);
		m_StandardPages.clear();

		GetParentDevice()->GetLinearPageManager(m_AllocatorType).DiscardLargePages(FenceID, QueueType, m_LargePages);
		m_LargePages.clear();
	}

	FAllocation LinearAllocator::AllocateLargePage(size_t SizeInBytes)
	{
		// Diagnostics are kept out of allocator core logic.
		if (m_AllocatorType == ELinearAllocatorType::CpuWritable)
			D3D12UploadWCDiagnostics_OnAllocateLargePage(L"CpuWritable", SizeInBytes);

		LinearAllocationPage* Page = GetParentDevice()->GetLinearPageManager(m_AllocatorType).CreateNewPage(SizeInBytes);
		m_LargePages.push_back(Page);

		FAllocation allocation;
		allocation.Resource = Page;
		allocation.Offset = 0;
		allocation.CPU = (uint8_t*)Page->GetResourceBaseAddress();
		allocation.GpuAddress = Page->GetGPUVirtualAddress();
		return allocation;
	}

}