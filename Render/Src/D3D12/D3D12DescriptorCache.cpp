#include "D3D12/D3D12DescriptorCache.h"

namespace RenderCore
{

	uint32_t GetTypeHash(const FD3D12SamplerArrayDesc& Key)
	{
		return SSE4_CRC32((void*)Key.SamplerID, Key.Count * sizeof(Key.SamplerID[0]));
	}

	FD3D12OnlineHeap::FD3D12OnlineHeap(std::weak_ptr< FD3D12Device> Device, FRHIGPUMask Node, bool CanLoopAround, FD3D12DescriptorCache* _Parent /*= nullptr*/)
		: FD3D12DeviceChild(Device)
		, Parent(_Parent)
		, DescriptorSize(0)
		, NextSlotIndex(0)
		, FirstUsedSlot(0)
		, Desc({})
		, bCanLoopAround(CanLoopAround)
	{};

}