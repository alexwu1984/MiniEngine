#pragma once
#include "D3D12/D3D12Resource.h"

namespace RenderCore
{
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
}