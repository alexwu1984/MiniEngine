#include "D3D12/D3D12DirectCommandListManager.h"

namespace RenderCore
{

	D3D12Fence::D3D12Fence(std::weak_ptr<D3D12Adapter> InParent, const std::wstring& InName /*= L"<unnamed>"*/)
		:D3D12AdapterChild(InParent)
	{

	}

	D3D12Fence::~D3D12Fence()
	{

	}

	void D3D12Fence::CreateFence()
	{

	}

	uint64_t D3D12Fence::Signal(ED3D12CommandQueueType InQueueType)
	{
		return 0;
	}

	void D3D12Fence::GpuWait(uint32_t DeviceGPUIndex, ED3D12CommandQueueType InQueueType, uint64_t FenceValue, uint32_t FenceGPUIndex)
	{

	}

	void D3D12Fence::GpuWait(ED3D12CommandQueueType InQueueType, uint64_t FenceValue)
	{

	}

	bool D3D12Fence::IsFenceComplete(uint64_t FenceValue)
	{
		return false;
	}

	void D3D12Fence::WaitForFence(uint64_t FenceValue)
	{

	}

}