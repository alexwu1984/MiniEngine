#pragma once
#include "D3D12/D3D12Resource.h"

namespace RenderCore
{
	const static uint32_t DEFAULT_ALIGN = 256;
	const static uint32_t GpuAllocatorPageSize = 0x10000;	// 64k
	const static uint32_t CpuAllocatorPageSize = 0x200000;	// 2MB

	struct FAllocation
	{
		ID3D12Resource* D3d12Resource;
		size_t Offset;
		void* CPU;
		D3D12_GPU_VIRTUAL_ADDRESS GpuAddress;
	};

	class FD3D12ResourceAllocator : public FD3D12DeviceChild
	{
	public:
		FD3D12ResourceAllocator(std::weak_ptr<FD3D12Device> ParentDevice,
			D3D12_DESCRIPTOR_HEAP_TYPE Type);

		~FD3D12ResourceAllocator() = default;

		D3D12_CPU_DESCRIPTOR_HANDLE Allocate(uint32_t Count);
		uint32_t GetDescriptorSize() const { return DescriptorSize; }

		static void DestroyAll();

	protected:
		static const uint32_t sm_NumDescriptorsPerHeap = 256;
		static std::vector<win32::com_ptr<ID3D12DescriptorHeap> > sm_DescriptorPool;
		static ID3D12DescriptorHeap* RequestNewHeap(std::shared_ptr<FD3D12Device> InDevice, D3D12_DESCRIPTOR_HEAP_TYPE Type);

	protected:
		D3D12_DESCRIPTOR_HEAP_TYPE HeapType;
		ID3D12DescriptorHeap* CurrentHeap;
		D3D12_CPU_DESCRIPTOR_HANDLE CurrentCpuAddress;
		uint32_t DescriptorSize;
		uint32_t RemainingFreeHandles;
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

	private:
		uint64_t FenceValue = 0;
	};

	class LinearAllocationPageManager : public FD3D12DeviceChild
	{
	public:
		LinearAllocationPageManager(std::weak_ptr<FD3D12Device> ParentDevice);
		LinearAllocationPage* RequestPage();
		void DiscardStandardPages(uint64_t FenceID, const std::vector<LinearAllocationPage*>& Pages);
		void DiscardLargePages(uint64_t FenceID, const std::vector<LinearAllocationPage*>& Pages);
		LinearAllocationPage* CreateNewPage(size_t SizeInBytes = 0);
		void Destroy();
		ELinearAllocatorType GetAllocatorType() const;

	private:
		using PagePool = std::queue<LinearAllocationPage* >;

		PagePool RetiredPages;
		PagePool LargePagePool;
		PagePool StandardPagePool;

		static ELinearAllocatorType ms_TypeCounter;
		ELinearAllocatorType AllocatorType;
	};

	class LinearAllocator : public FD3D12DeviceChild
	{
	public:
		LinearAllocator(ELinearAllocatorType Type, std::weak_ptr<FD3D12Device> ParentDevice);
		FAllocation Allocate(size_t SizeInBytes, size_t Alignment = DEFAULT_ALIGN);
		void CleanupUsedPages(uint64_t FenceID);

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