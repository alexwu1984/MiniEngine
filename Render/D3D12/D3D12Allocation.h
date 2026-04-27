#pragma once
#include "D3D12/D3D12Resource.h"

#include <mutex>
#include <map>
#include <queue>
#include <utility>
#include <vector>

namespace RenderCore
{
	const static uint32_t DEFAULT_ALIGN = 256;
	const static uint32_t GpuAllocatorPageSize = 0x10000;	// 64k

	// UE 4.26 FD3D12Device: DefaultFastAllocator(..., D3D12_HEAP_TYPE_UPLOAD, 1024 * 1024 * 4)
	// Align UploadFastAllocator linear page size with Epic's default fast-allocator page (4 MiB).
	const static uint32_t CpuAllocatorPageSize = 4u * 1024u * 1024u;

	// UE 4.26 D3D12Allocation.h — buddy / placed-buffer path constants (for parity when suballocating placed buffers).
	static constexpr uint32_t UE426_MIN_PLACED_BUFFER_SIZE = 64u * 1024u;
	static constexpr uint32_t UE426_D3D_BUFFER_ALIGNMENT = 64u * 1024u;

	class FD3D12UploadPlacedBuddyPool;
	class FD3D12FastAllocatorPage;

	struct FAllocation
	{
		// Backing page resource (when allocating from FD3D12FastAllocatorPage pools).
		FD3D12FastAllocatorPage* Resource = nullptr;
		// Backing D3D12 resource (valid for both fast-allocator pages and transient upload ring slices).
		ID3D12Resource* D3D12Resource = nullptr;
		size_t Offset = 0;
		void* CPU = nullptr;
		D3D12_GPU_VIRTUAL_ADDRESS GpuAddress = 0;
	};

	// UE-style fence-aware ring buffer (fixed-size; waits on wrap instead of growing).
	class FD3D12AbstractRingBuffer
	{
	public:
		explicit FD3D12AbstractRingBuffer(uint64_t BufferSize)
			: Fence(nullptr)
			, Size(BufferSize)
			, Head(0)
			, Tail(0)
			, UsedSize(0)
			, LastFence(0)
		{}

		static const uint64_t FailedReturnValue = uint64_t(-1);

		void Reset(uint64_t NewSize)
		{
			Size = NewSize;
			Head = 0;
			Tail = 0;
			UsedSize = 0;
			LastFence = 0;
			OutstandingAllocs.clear();
		}

		void SetFence(FD3D12Fence* InFence)
		{
			Fence = InFence;
			LastFence = 0;
		}

		uint64_t Allocate(uint64_t Count);
		uint64_t AllocateOrWait(uint64_t Count);

		uint64_t GetSize() const { return Size; }
		uint64_t GetHead() const { return Head; }
		uint64_t GetTail() const { return Tail; }
		uint64_t GetUsedSize() const { return UsedSize; }
		std::size_t GetOutstandingFenceCount() const { return OutstandingAllocs.size(); }

	private:
		void UpdateCompleted();
		uint64_t OldestOutstandingFenceValue() const;

	private:
		FD3D12Fence* Fence;
		uint64_t Size;
		uint64_t Head;
		uint64_t Tail;
		uint64_t UsedSize;
		uint64_t LastFence;
		std::map<uint64_t, uint64_t> OutstandingAllocs; // fenceValue -> blocks
	};

	// Fixed-size transient UPLOAD ring used for dynamic constant/uniform data.
	// Matches UE strategy: bounded size, fence-aware reuse, wait on wrap.
	class FD3D12TransientUploadRing : public FD3D12AdapterChild
	{
	public:
		explicit FD3D12TransientUploadRing(std::weak_ptr<FD3D12Adapter> InParentAdapter);
		~FD3D12TransientUploadRing();

		bool Initialize(uint64_t BufferBytes);
		void Destroy();

		// Allocate from the ring (bytes, alignment). Blocks (waits) when the ring is full.
		// Returned allocation uses Offset within the ring buffer resource.
		FAllocation Allocate(uint64_t Bytes, uint64_t Alignment = DEFAULT_ALIGN);

		ID3D12Resource* GetResource() const { return UploadResource.get(); }
		void* GetMappedBase() const { return MappedBase; }
		uint64_t GetSizeBytes() const { return SizeBytes; }
		uint64_t GetHeadBytes() const { return Ring.GetHead(); }
		uint64_t GetTailBytes() const { return Ring.GetTail(); }
		uint64_t GetUsedBytes() const { return Ring.GetUsedSize(); }
		std::size_t GetOutstandingFenceCount() const { return Ring.GetOutstandingFenceCount(); }

	private:
		uint64_t AlignUp(uint64_t v, uint64_t a) const { return (a == 0) ? v : ((v + (a - 1)) & ~(a - 1)); }

	private:
		win32::com_ptr<ID3D12Resource> UploadResource;
		void* MappedBase = nullptr;
		uint64_t SizeBytes = 0;
		FD3D12AbstractRingBuffer Ring;
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
		// Non-shader-visible CPU pools; larger blocks reduce heap count under heavy binding churn.
		static const uint32_t sm_NumDescriptorsPerHeap = 1024;
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

	// UE4-style backing page for FD3D12FastAllocator / FD3D12LinearAllocator suballocations.
	class FD3D12FastAllocatorPage : public FD3D12Resource
	{
		friend class FD3D12LinearAllocator;

	public:
		FD3D12FastAllocatorPage(std::weak_ptr<FD3D12Device> ParentDevice,
			ID3D12Resource* InResource,
			D3D12_RESOURCE_STATES InitialState,
			D3D12_RESOURCE_DESC const& InDesc,
			D3D12_HEAP_TYPE InHeapType = D3D12_HEAP_TYPE_DEFAULT);
		~FD3D12FastAllocatorPage();

		uint64_t GetFenceValue() const { return FenceValue; }
		void SetFenceValue(uint64_t InFenceValue) { FenceValue = InFenceValue; }

		ED3D12CommandQueueType GetRetireQueueType() const { return RetireQueueType; }
		void SetRetireQueueType(ED3D12CommandQueueType InQueueType) { RetireQueueType = InQueueType; }

		void BindPlacedBuddy(FD3D12UploadPlacedBuddyPool* Pool, uint32_t OffsetMinUnits, uint32_t Order, EFastAllocatorType OwnerAllocatorType);

	private:
		uint64_t FenceValue = 0;
		ED3D12CommandQueueType RetireQueueType = ED3D12CommandQueueType::Default;

		FD3D12UploadPlacedBuddyPool* PlacedBuddyPool = nullptr;
		uint32_t PlacedBuddyOffsetMinUnits = 0;
		uint32_t PlacedBuddyOrder = 0;
		EFastAllocatorType PlacedBuddyOwnerAllocatorType = InvalidFastAllocator;
		bool bHasPlacedBuddyBinding = false;
	};

	// UE4-style FD3D12FastAllocator: owns standard linear pages (UPLOAD / DEFAULT) for FD3D12LinearAllocator.
	class FD3D12FastAllocator : public FD3D12DeviceChild
	{
	public:
		FD3D12FastAllocator(std::weak_ptr<FD3D12Device> ParentDevice);
		FD3D12FastAllocatorPage* RequestPage();
		void DiscardStandardPages(uint64_t FenceID, ED3D12CommandQueueType QueueType, const std::vector<FD3D12FastAllocatorPage*>& Pages);
		void DiscardLargePages(uint64_t FenceID, ED3D12CommandQueueType QueueType, const std::vector<FD3D12FastAllocatorPage*>& Pages);
		FD3D12FastAllocatorPage* CreateNewPage(size_t SizeInBytes = 0);
		void Destroy();
		EFastAllocatorType GetAllocatorType() const;
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

		void EnqueuePlacedBuddyFree(FD3D12UploadPlacedBuddyPool* Pool, uint32_t OffsetMinUnits, uint32_t Order, uint64_t FenceValue, ED3D12CommandQueueType QueueType);

	private:
		using PagePool = std::queue<FD3D12FastAllocatorPage* >;

		struct FLargePageDelete
		{
			uint64_t FenceValue = 0;
			ED3D12CommandQueueType QueueType = ED3D12CommandQueueType::Default;
			FD3D12FastAllocatorPage* Page = nullptr;
		};

		// Retired pages are tracked per D3D12 queue to preserve monotonic fence ordering.
		// This enables O(1) recycling ("while front complete") like MiniEngine/DEMO.
		PagePool RetiredPages[3]; // [QueueTypeIndex]
		PagePool ReadyPages;
		std::queue<FLargePageDelete> LargePageDeletionQueue;
		// Owns one ref for every standard page ever created by this manager.
		// Availability is tracked separately via ReadyPages/RetiredPages.
		std::vector<FD3D12FastAllocatorPage*> OwnedStandardPages;

		static EFastAllocatorType ms_TypeCounter;
		EFastAllocatorType AllocatorType;

		struct FPlacedBuddyPendingFree
		{
			FD3D12UploadPlacedBuddyPool* Pool = nullptr;
			uint32_t OffsetMinUnits = 0;
			uint32_t Order = 0;
			uint64_t FenceValue = 0;
			ED3D12CommandQueueType QueueType = ED3D12CommandQueueType::Default;
		};

		mutable std::mutex PlacedBuddyDeferredMutex;
		std::vector<FPlacedBuddyPendingFree> PlacedBuddyDeferred;

		void ProcessPlacedBuddyDeferredFrees();
		void DrainPlacedBuddyDeferredWithWait();
	};

	class FD3D12LinearAllocator : public FD3D12DeviceChild
	{
	public:
		FD3D12LinearAllocator(EFastAllocatorType Type, std::weak_ptr<FD3D12Device> ParentDevice);
		FAllocation Allocate(size_t SizeInBytes, size_t Alignment = DEFAULT_ALIGN);
		void CleanupUsedPages(uint64_t FenceID, ED3D12CommandQueueType QueueType);

	private:
		FAllocation AllocateLargePage(size_t SizeInBytes);

		EFastAllocatorType m_AllocatorType;
		size_t m_PageSize;
		size_t m_CurrentOffset;

		std::vector<FD3D12FastAllocatorPage*> m_StandardPages;
		std::vector<FD3D12FastAllocatorPage*> m_LargePages;

		FD3D12FastAllocatorPage* m_CurrentPage;
	};
}
