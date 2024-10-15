#include "D3D12/D3D12WindowDevice.h"
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12DirectCommandListManager.h"

namespace RenderCore
{

	D3D12Device::D3D12Device(std::weak_ptr<D3D12Adapter> InAdapter)
		:D3D12AdapterChild(InAdapter)
	{

	}

	D3D12Device::~D3D12Device()
	{

	}

	void D3D12Device::Initialize()
	{

	}

	void D3D12Device::CreateCommandContexts()
	{

	}

	void D3D12Device::InitPlatformSpecific()
	{

	}

	void D3D12Device::Cleanup()
	{

	}

	ID3D12Device* D3D12Device::GetDevice()
	{
		return GetParentAdapter()->GetD3DDevice();
	}

	ID3D12CommandQueue* D3D12Device::GetD3DCommandQueue(ED3D12CommandQueueType InQueueType /*= ED3D12CommandQueueType::Default*/) const
	{
		return nullptr;
	}

}