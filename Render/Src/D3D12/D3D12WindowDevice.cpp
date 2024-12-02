#include "D3D12/D3D12WindowDevice.h"
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12DirectCommandListManager.h"

namespace RenderCore
{

	FD3D12Device::FD3D12Device(std::weak_ptr<FD3D12Adapter> InAdapter)
		:FD3D12AdapterChild(InAdapter)
		,RTVAllocator(FRHIGPUMask(1), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256)
		,DSVAllocator(FRHIGPUMask(1),D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 256)
		,SRVAllocator(FRHIGPUMask(1),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024)
		,UAVAllocator(FRHIGPUMask(1),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024)
		,SamplerAllocator(FRHIGPUMask(1),FD3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 128)
	{

	}

	FD3D12Device::~FD3D12Device()
	{

	}

	void FD3D12Device::Initialize()
	{
		SetupAfterDeviceCreation();
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

	void FD3D12Device::SetupAfterDeviceCreation()
	{
		ID3D12Device* Direct3DDevice = GetParentAdapter()->GetD3DDevice();

		// Init offline descriptor allocators
		RTVAllocator.Init(Direct3DDevice);
		DSVAllocator.Init(Direct3DDevice);
		SRVAllocator.Init(Direct3DDevice);
		UAVAllocator.Init(Direct3DDevice);
		SamplerAllocator.Init(Direct3DDevice);
	}

}