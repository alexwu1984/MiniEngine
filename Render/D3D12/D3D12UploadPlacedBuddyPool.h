#pragma once

#include "D3D12/D3D12RHICommon.h"
#include "win/com_ptr.h"

#include <cstdint>
#include <mutex>
#include <set>
#include <vector>

namespace RenderCore
{
	class FD3D12Device;

	// UE 4.26-style kPlacedResource buddy sub-alloc over a single ID3D12Heap (UPLOAD, buffers-only).
	// Used for standard UploadFastAllocator linear pages to reduce implicit committed heaps / WC churn.
	class FD3D12UploadPlacedBuddyPool : public FD3D12DeviceChild
	{
	public:
		explicit FD3D12UploadPlacedBuddyPool(std::weak_ptr<FD3D12Device> InParent);
		~FD3D12UploadPlacedBuddyPool();

		bool Initialize(uint64_t HeapSizeBytes);
		void Destroy();

		bool TryAllocatePlacedUploadPage(uint64_t PageSizeBytes, win32::com_ptr<ID3D12Resource>& OutResource,
			uint64_t& OutGpuVA, void*& OutCpuMapped, uint32_t& OutOffsetMinUnits, uint32_t& OutOrder);

		void DeallocateBlock(uint32_t OffsetInMinUnits, uint32_t Order);

	private:
		static constexpr uint32_t kMinBlockBytes = 64u * 1024u;

		uint32_t SizeToUnitSize(uint32_t SizeBytes) const;
		uint32_t UnitSizeToOrder(uint32_t UnitSize) const;
		uint32_t OrderToUnitSize(uint32_t Order) const { return 1u << Order; }
		uint32_t GetBuddyOffset(uint32_t Offset, uint32_t SizeInUnits) const { return Offset ^ SizeInUnits; }

		uint32_t AllocateBlock(uint32_t Order);
		void DeallocateBlockInternal(uint32_t Offset, uint32_t Order);

		std::recursive_mutex CS;
		win32::com_ptr<ID3D12Heap> Heap;
		uint64_t BackingHeapSizeBytes = 0;
		uint32_t MaxOrder = 0;
		std::vector<std::set<uint32_t>> FreeBlocks;
		bool bInitialized = false;
	};
}
