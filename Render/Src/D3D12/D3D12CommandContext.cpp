#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12ReourceTraits.h"
#include "D3D12/D3D12CommandList.h"

namespace RenderCore
{

	FD3D12CommandContextBase::FD3D12CommandContextBase(std::weak_ptr<FD3D12Adapter> InParent, bool InIsDefaultContext, bool InIsAsyncComputeContext)
		:FD3D12AdapterChild(InParent),
		bIsDefaultContext(InIsDefaultContext),
		bIsAsyncComputeContext(InIsAsyncComputeContext)
	{

	}

	D3D12CommandContext::D3D12CommandContext(std::weak_ptr<FD3D12Device> InParent, bool InIsDefaultContext, bool InIsAsyncComputeContext)
		:FD3D12CommandContextBase(InParent.lock()->GetParentAdapter(),InIsDefaultContext,InIsAsyncComputeContext),
		FD3D12DeviceChild(InParent),
		CommandAllocator(nullptr),
		CommandAllocatorManager(InParent, InIsAsyncComputeContext ? D3D12_COMMAND_LIST_TYPE_COMPUTE : D3D12_COMMAND_LIST_TYPE_DIRECT)
	{

	}

	D3D12CommandContext::~D3D12CommandContext()
	{
	}

	void D3D12CommandContext::SetViewPort(int32_t TopLeftX, int32_t TopLeftY, int32_t SizeX, int32_t SizeY)
	{
		Assert(CommandListHandle.GraphicsCommandList());
		D3D12_VIEWPORT vp;
		vp.Width = SizeX;
		vp.Height = SizeY;
		vp.MinDepth = 0;
		vp.MaxDepth = 1;
		vp.TopLeftX = TopLeftX;
		vp.TopLeftY = TopLeftY;
		CommandListHandle.GraphicsCommandList()->RSSetViewports(1, &vp);

		CD3DX12_RECT ScissorRect(TopLeftX, TopLeftY, SizeX, SizeY);
		CommandListHandle.GraphicsCommandList()->RSSetScissorRects(1, &ScissorRect);
	}

	void D3D12CommandContext::SetRenderTarget(std::vector<std::shared_ptr<RHITexture2D>> Targets, std::shared_ptr< RHITexture2D> Depth)
	{
		Assert(CommandListHandle.GraphicsCommandList());
		auto DepthRHI = RHIResourceCast(Depth.get());
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> D3D12TargetViews;
		for (auto Target : Targets)
		{
			auto RenderTargetRHI = RHIResourceCast(Target.get());
			if (RenderTargetRHI && RenderTargetRHI->GetRTV().ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
			{
				TransitionResource(RenderTargetRHI->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
				D3D12TargetViews.emplace_back(RenderTargetRHI->GetRTV());
			}
		}
		D3D12_CPU_DESCRIPTOR_HANDLE DSV{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
		if (DepthRHI)
		{
			DSV = DepthRHI->GetRTV();
			TransitionResource(DepthRHI->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		}
		CommandListHandle.GraphicsCommandList()->OMSetRenderTargets(D3D12TargetViews.size(), D3D12TargetViews.data(), FALSE, DepthRHI ? &DSV : nullptr);
	}

	void D3D12CommandContext::SetRenderTarget(std::shared_ptr<RHITexture2D> Tex, std::shared_ptr< RHITexture2D> Depth)
	{
		std::vector<std::shared_ptr<RHITexture2D>> Targets{ Tex };
		SetRenderTarget(Targets, Depth);
	}

	void D3D12CommandContext::Clear(std::shared_ptr<RHITexture2D> RenderTarget, std::shared_ptr<RHITexture2D> DepthTarget,
									const core::FLinearColor& Color, float Depth /*= 1.0f*/, uint8_t Stencil /*= 0*/)
	{
		Assert(CommandListHandle.GraphicsCommandList());
		auto TexRHI = RHIResourceCast(RenderTarget.get());
		auto DepthRHI = RHIResourceCast(DepthTarget.get());
		if(RenderTarget)
			CommandListHandle.GraphicsCommandList()->ClearRenderTargetView(TexRHI->GetRTV(), &Color.R, 0, nullptr);
		if(DepthRHI)
			CommandListHandle.GraphicsCommandList()->ClearDepthStencilView(DepthRHI->GetRTV(), D3D12_CLEAR_FLAG_DEPTH,Depth,Stencil,0,nullptr);
	}

	void D3D12CommandContext::RHIBeing()
	{
		Assert(CommandAllocator);
		if (CommandAllocator)
			CommandListHandle.Reset(*CommandAllocator);
		
	}

	uint64_t D3D12CommandContext::Finish(bool WaitForCompletion /*= false*/)
	{
		CommandListHandle.Execute(WaitForCompletion);
		return CommandListHandle.CurrentGeneration();
	}

	FD3D12CommandListManager& D3D12CommandContext::GetCommandListManager()
	{
		return bIsAsyncComputeContext ? GetParentDevice()->GetAsyncCommandListManager() : GetParentDevice()->GetCommandListManager();
	}

	void D3D12CommandContext::ConditionalObtainCommandAllocator()
	{
		if (CommandAllocator == nullptr)
		{
			// Obtain a command allocator if the context doesn't already have one.
			// This will check necessary fence values to ensure the returned command allocator isn't being used by the GPU, then reset it.
			CommandAllocator = CommandAllocatorManager.ObtainCommandAllocator();
		}
	}

	void D3D12CommandContext::OpenCommandList()
	{
		// Conditionally get a new command allocator.
		// Each command context uses a new allocator for all command lists within a "frame".
		ConditionalObtainCommandAllocator();

		// Get a new command list
		CommandListHandle = GetCommandListManager().ObtainCommandList(*CommandAllocator);
		CommandListHandle.SetCurrentOwningContext(this);

		numDraws = 0;
		numDispatches = 0;
		numClears = 0;
		numBarriers = 0;
		numCopies = 0;
		otherWorkCounter = 0;
	}

	void D3D12CommandContext::CloseCommandList()
	{
		CommandListHandle.Close();
	}

	void D3D12CommandContext::TransitionResource(FD3D12Resource* Resource, D3D12_RESOURCE_STATES NewState, bool Flush /*= false*/)
	{
		bool NeedTransition = false;
		for (size_t i = 0; i < Resource->GetSubresourceCount(); ++i)
		{
			D3D12_RESOURCE_STATES OldState = Resource->GetResourceState().GetSubresourceState(i);
			if (NewState != OldState)
			{
				NeedTransition = true;
				break;
			}
		}

		if (NeedTransition)
		{
			CommandListHandle.AddTransitionBarrier(Resource, Resource->GetResourceState().GetSubresourceState(0), NewState,0);
		}

		if (Flush )
		{
			CommandListHandle.FlushResourceBarriers();
		}

	}

}
