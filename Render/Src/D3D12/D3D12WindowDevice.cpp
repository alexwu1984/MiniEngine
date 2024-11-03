#include "D3D12/D3D12WindowDevice.h"
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12DirectCommandListManager.h"

namespace RenderCore
{

	FD3D12Device::FD3D12Device(std::weak_ptr<FD3D12Adapter> InAdapter)
		:FD3D12AdapterChild(InAdapter)
	{

	}

	FD3D12Device::~FD3D12Device()
	{

	}

	void FD3D12Device::Initialize()
	{

	}

	void FD3D12Device::CreateCommandContexts()
	{

	}

	void FD3D12Device::InitPlatformSpecific()
	{

	}

	void FD3D12Device::Cleanup()
	{

	}

	ID3D12Device* FD3D12Device::GetDevice()
	{
		return GetParentAdapter()->GetD3DDevice();
	}

	ID3D12CommandQueue* FD3D12Device::GetD3DCommandQueue(ED3D12CommandQueueType InQueueType /*= ED3D12CommandQueueType::Default*/) const
	{
		return nullptr;
	}

	void FD3D12Device::BlockUntilIdle()
	{
		//GetDefaultCommandContext().FlushCommands();

		//if (GEnableAsyncCompute)
		//{
		//	GetDefaultAsyncComputeContext().FlushCommands();
		//}

		GetCommandListManager().WaitForCommandQueueFlush();
		//GetCopyCommandListManager().WaitForCommandQueueFlush();
		//GetAsyncCommandListManager().WaitForCommandQueueFlush();
	}

}