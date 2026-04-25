#include "D3D12/D3D12WindowDevice.h"
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12Allocation.h"
#include "D3D12/D3D12DescriptorCache.h"

namespace RenderCore
{

	FD3D12Device::FD3D12Device(std::weak_ptr<FD3D12Adapter> InAdapter)
		:FD3D12AdapterChild(InAdapter)
		, DynamicDescriptorHeapPools(std::make_unique<FDynamicDescriptorHeapPoolsPerDevice>())
	{

	}

	FD3D12Device::~FD3D12Device()
	{

	}

	void FD3D12Device::Initialize()
	{
		CreateCommandContexts();
		InitPlatformSpecific();
		InitDescriptorAllocator();

		if(DefaultCommandContext)
			DefaultCommandContext->OpenCommandList();
		if(AsyncComputeContext)
			AsyncComputeContext->OpenCommandList();
	}

	void FD3D12Device::CreateCommandContexts()
	{
		DefaultCommandContext = std::make_shared<D3D12CommandContext>(this->shared_from_this(), true, false);
		DefaultCommandContext->Initialize();
		AsyncComputeContext = std::make_shared<D3D12CommandContext>(this->shared_from_this(), false, true);
		AsyncComputeContext->Initialize();
	}

	void FD3D12Device::InitPlatformSpecific()
	{
		CommandListManager = std::make_shared<FD3D12CommandListManager>(this->shared_from_this(), D3D12_COMMAND_LIST_TYPE_DIRECT, ED3D12CommandQueueType::Default);
		CopyCommandListManager = std::make_shared<FD3D12CommandListManager>(this->shared_from_this(), D3D12_COMMAND_LIST_TYPE_COPY, ED3D12CommandQueueType::Copy);
		AsyncCommandListManager = std::make_shared<FD3D12CommandListManager>(this->shared_from_this(), D3D12_COMMAND_LIST_TYPE_COMPUTE, ED3D12CommandQueueType::Async);

		CommandListManager->Create(L"3D Queue");
		CopyCommandListManager->Create(L"Copy Queue");
		AsyncCommandListManager->Create(L"Async Compute Queue", 0, 0);

		PageManager[0] = std::make_shared<LinearAllocationPageManager>(this->shared_from_this());
		PageManager[1] = std::make_shared<LinearAllocationPageManager>(this->shared_from_this());
	}

	void FD3D12Device::InitDescriptorAllocator()
	{
		DescriptorAllocator[0] = std::make_shared<FD3D12ResourceAllocator>(this->shared_from_this(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		DescriptorAllocator[1] = std::make_shared<FD3D12ResourceAllocator>(this->shared_from_this(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		DescriptorAllocator[2] = std::make_shared<FD3D12ResourceAllocator>(this->shared_from_this(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		DescriptorAllocator[3] = std::make_shared<FD3D12ResourceAllocator>(this->shared_from_this(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	}

	void FD3D12Device::Cleanup()
	{
		if (CommandListManager)
			CommandListManager->Destroy();
		if (CopyCommandListManager)
			CopyCommandListManager->Destroy();
		if (AsyncCommandListManager)
			AsyncCommandListManager->Destroy();

		if (DefaultCommandContext)
			DefaultCommandContext->Destroy();
		if (AsyncComputeContext)
			AsyncComputeContext->Destroy();
		DefaultCommandContext = {};
		AsyncComputeContext = {};
		CommandListManager = {};
		CopyCommandListManager = {};
		AsyncCommandListManager = {};

		if (PageManager[0])
		{
			PageManager[0]->Destroy();
			PageManager[0] = {};
		}
		if (PageManager[1])
		{
			PageManager[1]->Destroy();
			PageManager[1] = {};
		}

		DescriptorAllocator[0] = {};
		DescriptorAllocator[1] = {};
		DescriptorAllocator[2] = {};
		DescriptorAllocator[3] = {};

		FD3D12ResourceAllocator::DestroyAll();
		if (DynamicDescriptorHeapPools != nullptr)
			DynamicDescriptorHeapPools->Clear();
	}

	FDynamicDescriptorHeapPoolsPerDevice& FD3D12Device::GetDynamicDescriptorHeapPools()
	{
		Assert(DynamicDescriptorHeapPools != nullptr);
		return *DynamicDescriptorHeapPools;
	}

	ID3D12Device* FD3D12Device::GetDevice()
	{
		return GetParentAdapter()->GetD3DDevice();
	}

	ID3D12CommandQueue* FD3D12Device::GetD3DCommandQueue(ED3D12CommandQueueType InQueueType /*= ED3D12CommandQueueType::Default*/) const
	{
		switch (InQueueType)
		{
		case ED3D12CommandQueueType::Default:
			Assert(CommandListManager->GetQueueType() == InQueueType);
			return CommandListManager->GetD3DCommandQueue();
		case ED3D12CommandQueueType::Async:
			Assert(AsyncCommandListManager->GetQueueType() == InQueueType);
			return AsyncCommandListManager->GetD3DCommandQueue();
		case ED3D12CommandQueueType::Copy:
			Assert(CopyCommandListManager->GetQueueType() == InQueueType);
			return CopyCommandListManager->GetD3DCommandQueue();
		default:
			Assert(false);
			return nullptr;
		}
	}

	FD3D12CommandListManager& FD3D12Device::GetCommandListManager(ED3D12CommandQueueType InQueueType /*= ED3D12CommandQueueType::Default*/) const
	{
		switch (InQueueType)
		{
		case ED3D12CommandQueueType::Default:
			Assert(CommandListManager->GetQueueType() == InQueueType);
			return *CommandListManager;
		case ED3D12CommandQueueType::Async:
			Assert(AsyncCommandListManager->GetQueueType() == InQueueType);
			return *AsyncCommandListManager;
		case ED3D12CommandQueueType::Copy:
			Assert(CopyCommandListManager->GetQueueType() == InQueueType);
			return *CopyCommandListManager;
		default:
			return *CommandListManager;
		}
	}

	LinearAllocationPageManager& FD3D12Device::GetLinearPageManager(ELinearAllocatorType InType) const
	{
		Assert(PageManager[0].get() && PageManager[1].get());
		switch (InType)
		{
		case ELinearAllocatorType::GpuExclusive:
			Assert(PageManager[0]->GetAllocatorType() == InType);
			return *PageManager[0];
		case ELinearAllocatorType::CpuWritable:
			Assert(PageManager[1]->GetAllocatorType() == InType);
			return *PageManager[1];
		default:
			return *PageManager[0];
		}
	}

	D3D12_CPU_DESCRIPTOR_HANDLE FD3D12Device::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t Count /*= 1*/)
	{
		Assert(DescriptorAllocator[Type].get());
		return DescriptorAllocator[Type]->Allocate(Count);
	}

	uint32_t FD3D12Device::GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE Type)
	{
		Assert(DescriptorAllocator[Type].get());
		return DescriptorAllocator[Type]->GetDescriptorSize();
	}

	void FD3D12Device::BlockUntilIdle()
	{
		if (DefaultCommandContext)
			DefaultCommandContext->FlushCommands(true);

		if (AsyncComputeContext)
			AsyncComputeContext->FlushCommands(true);

		if (CommandListManager)
			CommandListManager->WaitForCommandQueueFlush();

		if (CopyCommandListManager)
			CopyCommandListManager->WaitForCommandQueueFlush();

		if (AsyncCommandListManager)
			AsyncCommandListManager->WaitForCommandQueueFlush();
	}

}