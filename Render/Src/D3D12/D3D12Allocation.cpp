#include "D3D12/D3D12Allocation.h"
#include "D3D12/D3D12WindowDevice.h"

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

	}

	D3D12_CPU_DESCRIPTOR_HANDLE FD3D12ResourceAllocator::Allocate(uint32_t Count)
	{
		if (CurrentHeap == nullptr || RemainingFreeHandles < Count)
		{
			CurrentHeap = RequestNewHeap(GetParentDevice(),HeapType);
			CurrentCpuAddress = CurrentHeap->GetCPUDescriptorHandleForHeapStart();
			RemainingFreeHandles = sm_NumDescriptorsPerHeap;
			if (DescriptorSize == 0)
			{
				DescriptorSize = GetParentDevice()->GetDevice()->GetDescriptorHandleIncrementSize(HeapType);
			}
		}

		D3D12_CPU_DESCRIPTOR_HANDLE Result = CurrentCpuAddress;
		CurrentCpuAddress.ptr += Count * DescriptorSize;
		RemainingFreeHandles -= Count;

		return Result;
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
}