#include "D3D12/D3D12WindowDevice.h"
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12Allocation.h"

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
		AsyncComputeContext = std::make_shared<D3D12CommandContext>(this->shared_from_this(), false, true);
	}

	void FD3D12Device::InitPlatformSpecific()
	{
		CommandListManager = std::make_shared<FD3D12CommandListManager>(this->shared_from_this(), D3D12_COMMAND_LIST_TYPE_DIRECT, ED3D12CommandQueueType::Default);
		CopyCommandListManager = std::make_shared<FD3D12CommandListManager>(this->shared_from_this(), D3D12_COMMAND_LIST_TYPE_COPY, ED3D12CommandQueueType::Copy);
		AsyncCommandListManager = std::make_shared<FD3D12CommandListManager>(this->shared_from_this(), D3D12_COMMAND_LIST_TYPE_COMPUTE, ED3D12CommandQueueType::Async);

		CommandListManager->Create(L"3D Queue");
		CopyCommandListManager->Create(L"Copy Queue");
		AsyncCommandListManager->Create(L"Async Compute Queue", 0, 0);
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

		DefaultCommandContext = {};
		AsyncComputeContext = {};
		CommandListManager = {};
		CopyCommandListManager = {};
		AsyncCommandListManager = {};

		FD3D12ResourceAllocator::DestroyAll();
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

}