#include "D3D12/D3D12BuddyAllocator.h"
#include "D3D12/D3D12UploadWCDiagnostics.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12Adapter.h"

#include <intrin.h>

namespace RenderCore
{
	FD3D12BuddyAllocator::FD3D12BuddyAllocator(std::weak_ptr<FD3D12Device> InParent, eBuddyAllocationStrategy InStrategy)
		: FD3D12DeviceChild(InParent)
		, AllocationStrategy(InStrategy)
	{
		Assert(AllocationStrategy == eBuddyAllocationStrategy::kPlacedResourceStrategy);
	}

	FD3D12BuddyAllocator::~FD3D12BuddyAllocator()
	{
		Destroy();
	}

	bool FD3D12BuddyAllocator::Initialize(uint64_t HeapSizeBytes)
	{
		Assert(AllocationStrategy == eBuddyAllocationStrategy::kPlacedResourceStrategy);
		std::lock_guard<std::recursive_mutex> Lock(CS);
		if (bInitialized)
			return true;
		if (HeapSizeBytes == 0 || (HeapSizeBytes % kMinBlockBytes) != 0)
			return false;
		const uint32_t maxBlockBytes = (uint32_t)HeapSizeBytes;
		if ((uint64_t)maxBlockBytes != HeapSizeBytes)
			return false;
		const uint32_t ratio = maxBlockBytes / kMinBlockBytes;
		if (ratio == 0 || (ratio & (ratio - 1)) != 0)
			return false;

		ID3D12Device* Dev = GetParentDevice()->GetDevice();
		if (!Dev)
			return false;

		D3D12_HEAP_PROPERTIES HeapProps = {};
		HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProps.CreationNodeMask = 1;
		HeapProps.VisibleNodeMask = 1;

		D3D12_HEAP_DESC Desc = {};
		Desc.SizeInBytes = HeapSizeBytes;
		Desc.Properties = HeapProps;
		Desc.Alignment = 0;
		Desc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;

		if (FAILED(Dev->CreateHeap(&Desc, IID_PPV_ARGS(Heap.get_init_ref()))))
			return false;

		D3D12UploadWCDiagnostics_OnCreateUploadCommittedBuffer(L"BuddyAllocatorHeap", (std::size_t)HeapSizeBytes);

		BackingHeapSizeBytes = HeapSizeBytes;
		const uint32_t unitSize = maxBlockBytes / kMinBlockBytes;
		unsigned long idx = 0;
		_BitScanReverse(&idx, unitSize + unitSize - 1);
		MaxOrder = (uint32_t)idx;
		FreeBlocks.clear();
		FreeBlocks.resize(MaxOrder + 1);
		FreeBlocks[MaxOrder].insert(0);
		bInitialized = true;
		return true;
	}

	void FD3D12BuddyAllocator::Destroy()
	{
		std::lock_guard<std::recursive_mutex> Lock(CS);
		if (!bInitialized)
			return;
		FreeBlocks.clear();
		Heap.reset();
		bInitialized = false;
	}

	uint32_t FD3D12BuddyAllocator::SizeToUnitSize(uint32_t SizeBytes) const
	{
		return (SizeBytes + kMinBlockBytes - 1) / kMinBlockBytes;
	}

	uint32_t FD3D12BuddyAllocator::UnitSizeToOrder(uint32_t UnitSize) const
	{
		if (UnitSize <= 1)
			return 0;
		unsigned long Result = 0;
		_BitScanReverse(&Result, UnitSize + UnitSize - 1);
		return (uint32_t)Result;
	}

	uint32_t FD3D12BuddyAllocator::AllocateBlock(uint32_t Order)
	{
		if (Order > MaxOrder)
			return UINT32_MAX;

		if (FreeBlocks[Order].empty())
		{
			if (Order == MaxOrder)
				return UINT32_MAX;
			const uint32_t left = AllocateBlock(Order + 1);
			if (left == UINT32_MAX)
				return UINT32_MAX;
			const uint32_t size = OrderToUnitSize(Order);
			const uint32_t right = left + size;
			FreeBlocks[Order].insert(right);
			return left;
		}

		auto It = FreeBlocks[Order].begin();
		const uint32_t offset = *It;
		FreeBlocks[Order].erase(It);
		return offset;
	}

	void FD3D12BuddyAllocator::DeallocateBlockInternal(uint32_t Offset, uint32_t Order)
	{
		const uint32_t size = OrderToUnitSize(Order);
		const uint32_t buddy = GetBuddyOffset(Offset, size);

		auto ItBuddy = FreeBlocks[Order].find(buddy);
		if (ItBuddy != FreeBlocks[Order].end())
		{
			FreeBlocks[Order].erase(ItBuddy);
			DeallocateBlockInternal((Offset < buddy) ? Offset : buddy, Order + 1);
		}
		else
		{
			FreeBlocks[Order].insert(Offset);
		}
	}

	void FD3D12BuddyAllocator::DeallocateBlock(uint32_t OffsetInMinUnits, uint32_t Order)
	{
		std::lock_guard<std::recursive_mutex> Lock(CS);
		if (!bInitialized)
			return;
		DeallocateBlockInternal(OffsetInMinUnits, Order);
	}

	bool FD3D12BuddyAllocator::TryAllocatePlacedUploadPage(uint64_t PageSizeBytes, win32::com_ptr<ID3D12Resource>& OutResource,
		uint64_t& OutGpuVA, void*& OutCpuMapped, uint32_t& OutOffsetMinUnits, uint32_t& OutOrder)
	{
		Assert(AllocationStrategy == eBuddyAllocationStrategy::kPlacedResourceStrategy);
		std::lock_guard<std::recursive_mutex> Lock(CS);
		OutResource.reset();
		OutGpuVA = 0;
		OutCpuMapped = nullptr;
		OutOffsetMinUnits = 0;
		OutOrder = 0;
		if (!bInitialized || PageSizeBytes > UINT32_MAX)
			return false;
		const uint32_t sz = (uint32_t)PageSizeBytes;
		if (sz == 0 || (sz % kMinBlockBytes) != 0)
			return false;

		const uint32_t unitSize = SizeToUnitSize(sz);
		const uint32_t order = UnitSizeToOrder(unitSize);
		const uint32_t allocUnits = OrderToUnitSize(order);
		if ((uint64_t)allocUnits * (uint64_t)kMinBlockBytes < (uint64_t)sz)
			return false;

		const uint32_t offsetUnits = AllocateBlock(order);
		if (offsetUnits == UINT32_MAX)
			return false;

		OutOffsetMinUnits = offsetUnits;
		OutOrder = order;

		const uint64_t heapOffsetBytes = (uint64_t)offsetUnits * (uint64_t)kMinBlockBytes;

		D3D12_RESOURCE_DESC Rd = {};
		Rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		Rd.Alignment = 0;
		Rd.Width = PageSizeBytes;
		Rd.Height = 1;
		Rd.DepthOrArraySize = 1;
		Rd.MipLevels = 1;
		Rd.Format = DXGI_FORMAT_UNKNOWN;
		Rd.SampleDesc.Count = 1;
		Rd.SampleDesc.Quality = 0;
		Rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		Rd.Flags = D3D12_RESOURCE_FLAG_NONE;

		ID3D12Device* Dev = GetParentDevice()->GetDevice();
		win32::com_ptr<ID3D12Resource> Res;
		if (FAILED(Dev->CreatePlacedResource(Heap.get(), heapOffsetBytes, &Rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(Res.get_init_ref()))))
		{
			DeallocateBlockInternal(offsetUnits, order);
			return false;
		}

		void* Cpu = nullptr;
		if (FAILED(Res->Map(0, nullptr, &Cpu)))
		{
			Res.reset();
			DeallocateBlockInternal(offsetUnits, order);
			return false;
		}

		D3D12UploadWCDiagnostics_OnUploadMap(L"LinearPage_BuddyAllocator", Cpu, (uint64_t)PageSizeBytes);
		D3D12UploadWCDiagnostics_OnCreateUploadCommittedBuffer(L"LinearPage_BuddyAllocator", (std::size_t)PageSizeBytes);

		Res->SetName(L"LinearPage_BuddyAllocator");
		OutResource = std::move(Res);
		OutGpuVA = OutResource->GetGPUVirtualAddress();
		OutCpuMapped = Cpu;
		return true;
	}
}
