#pragma once
#include "D3D12/D3D12Resource.h"

#include <queue>
#include <utility>
#include <vector>

namespace RenderCore
{
	const static uint32_t DEFAULT_ALIGN = 256;
	const static uint32_t GpuAllocatorPageSize = 0x10000;	// 64k
	const static uint32_t CpuAllocatorPageSize = 0x200000;	// 2MB

	class LinearAllocationPage;

	struct FAllocation
	{
		LinearAllocationPage* Resource;
		size_t Offset;
		void* CPU;
		D3D12_GPU_VIRTUAL_ADDRESS GpuAddress;
	};

	class FD3D12ResourceAllocator : public FD3D12DeviceChild
	{
	public:
		struct FDescriptorAllocation
		{
			ID3D12DescriptorHeap* Heap = nullptr;
			D3D12_CPU_DESCRIPTOR_HANDLE Cpu{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
			uint32_t Count = 0;
			uint32_t HeapIndex = 0;
			uint32_t Offset = 0;

			bool IsValid() const { return Heap != nullptr && Cpu.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL && Count > 0; }
		};

		FD3D12ResourceAllocator(std::weak_ptr<FD3D12Device> ParentDevice,
			D3D12_DESCRIPTOR_HEAP_TYPE Type);

		~FD3D12ResourceAllocator() = default;

		// Recyclable allocation used by textures/RTs to avoid descriptor heap leaks.
		FDescriptorAllocation AllocateBlock(uint32_t Count);
		void FreeBlock(const FDescriptorAllocation& Allocation);
		uint32_t GetDescriptorSize() const { return DescriptorSize; }
		std::size_t GetHeapCount() const { return Heaps.size(); }
		std::size_t GetFreeBlockCount() const
		{
			std::size_t Total = 0;
			for (const FHeapState& H : Heaps)
				Total += H.FreeList.size();
			return Total;
		}
		static std::size_t GetGlobalPoolSize() { return sm_DescriptorPool.size(); }

		static void DestroyAll();

	protected:
		// Same default as DirectX-Graphics-Samples MiniEngine Core/DescriptorHeap.h (DescriptorAllocator::sm_NumDescriptorsPerHeap).
		static const uint32_t sm_NumDescriptorsPerHeap = 256;
		static std::vector<win32::com_ptr<ID3D12DescriptorHeap> > sm_DescriptorPool;
		static ID3D12DescriptorHeap* RequestNewHeap(std::shared_ptr<FD3D12Device> InDevice, D3D12_DESCRIPTOR_HEAP_TYPE Type);

		struct FFreeBlock
		{
			uint32_t Offset = 0;
			uint32_t Count = 0;
		};
		struct FHeapState
		{
			win32::com_ptr<ID3D12DescriptorHeap> Heap;
			std::vector<FFreeBlock> FreeList; // sorted by Offset
		};

	protected:
		D3D12_DESCRIPTOR_HEAP_TYPE HeapType;
		ID3D12DescriptorHeap* CurrentHeap;
		D3D12_CPU_DESCRIPTOR_HANDLE CurrentCpuAddress;
		uint32_t DescriptorSize;
		uint32_t RemainingFreeHandles;

		// Recyclable heaps (CPU-only). Used by AllocateBlock/FreeBlock.
		std::vector<FHeapState> Heaps;
	};

	class LinearAllocationPage : public FD3D12Resource
	{
		friend class LinearAllocator;

	public:
		LinearAllocationPage(std::weak_ptr<FD3D12Device> ParentDevice,
			ID3D12Resource* InResource,
			D3D12_RESOURCE_STATES InitialState,
			D3D12_RESOURCE_DESC const& InDesc,
			D3D12_HEAP_TYPE InHeapType = D3D12_HEAP_TYPE_DEFAULT);
		~LinearAllocationPage();

		uint64_t GetFenceValue() const { return FenceValue; }
		void SetFenceValue(uint64_t InFenceValue) { FenceValue = InFenceValue; }

		ED3D12CommandQueueType GetRetireQueueType() const { return RetireQueueType; }
		void SetRetireQueueType(ED3D12CommandQueueType InQueueType) { RetireQueueType = InQueueType; }

	private:
		uint64_t FenceValue = 0;
		ED3D12CommandQueueType RetireQueueType = ED3D12CommandQueueType::Default;
	};

	class LinearAllocationPageManager : public FD3D12DeviceChild
	{
	public:
		LinearAllocationPageManager(std::weak_ptr<FD3D12Device> ParentDevice);
		LinearAllocationPage* RequestPage();
		void DiscardStandardPages(uint64_t FenceID, ED3D12CommandQueueType QueueType, const std::vector<LinearAllocationPage*>& Pages);
		void DiscardLargePages(uint64_t FenceID, ED3D12CommandQueueType QueueType, const std::vector<LinearAllocationPage*>& Pages);
		LinearAllocationPage* CreateNewPage(size_t SizeInBytes = 0);
		void Destroy();
		ELinearAllocatorType GetAllocatorType() const;
		std::size_t GetRetiredPageCount() const
		{
			return RetiredPages[0].size() + RetiredPages[1].size() + RetiredPages[2].size();
		}
		std::size_t GetReadyPageCount() const { return ReadyPages.size(); }
		std::size_t GetLargePageCount() const { return LargePageDeletionQueue.size(); }
		std::size_t GetStandardPageCount() const { return OwnedStandardPages.size(); }
		/** Per-queue retired standard pages: index matches ED3D12CommandQueueType (Default/Copy/Async). */
		std::size_t GetRetiredPageCountForQueue(int QueueTypeIndex) const
		{
			if (QueueTypeIndex < 0 || QueueTypeIndex > 2)
				return 0;
			return RetiredPages[QueueTypeIndex].size();
		}

	private:
		using PagePool = std::queue<LinearAllocationPage* >;

		struct FLargePageDelete
		{
			uint64_t FenceValue = 0;
			ED3D12CommandQueueType QueueType = ED3D12CommandQueueType::Default;
			LinearAllocationPage* Page = nullptr;
		};

		// Retired pages are tracked per D3D12 queue to preserve monotonic fence ordering.
		// This enables O(1) recycling ("while front complete") like MiniEngine/DEMO.
		PagePool RetiredPages[3]; // [QueueTypeIndex]
		PagePool ReadyPages;
		std::queue<FLargePageDelete> LargePageDeletionQueue;
		// Owns one ref for every standard page ever created by this manager.
		// Availability is tracked separately via ReadyPages/RetiredPages.
		std::vector<LinearAllocationPage*> OwnedStandardPages;

		static ELinearAllocatorType ms_TypeCounter;
		ELinearAllocatorType AllocatorType;
	};

	class LinearAllocator : public FD3D12DeviceChild
	{
	public:
		LinearAllocator(ELinearAllocatorType Type, std::weak_ptr<FD3D12Device> ParentDevice);
		FAllocation Allocate(size_t SizeInBytes, size_t Alignment = DEFAULT_ALIGN);
		void CleanupUsedPages(uint64_t FenceID, ED3D12CommandQueueType QueueType);

	private:
		FAllocation AllocateLargePage(size_t SizeInBytes);

		ELinearAllocatorType m_AllocatorType;
		size_t m_PageSize;
		size_t m_CurrentOffset;

		std::vector<LinearAllocationPage*> m_StandardPages;
		std::vector<LinearAllocationPage*> m_LargePages;

		LinearAllocationPage* m_CurrentPage;
	};
}