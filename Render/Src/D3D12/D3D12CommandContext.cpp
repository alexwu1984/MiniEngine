#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12ReourceTraits.h"
#include "D3D12/D3D12CommandList.h"
#include "D3D12/D3D12StateCache.h"

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
		CommandAllocatorManager(InParent, InIsAsyncComputeContext ? D3D12_COMMAND_LIST_TYPE_COMPUTE : D3D12_COMMAND_LIST_TYPE_DIRECT),
		CpuLinearAllocator(ELinearAllocatorType::CpuWritable, InParent),
		GpuLinearAllocator(ELinearAllocatorType::GpuExclusive, InParent)
	{
		StateCache = std::make_shared<FD3D12StateCache>(InParent);
	}

	D3D12CommandContext::~D3D12CommandContext()
	{
	}

	void D3D12CommandContext::SetViewPort(int32_t TopLeftX, int32_t TopLeftY, int32_t SizeX, int32_t SizeY)
	{
		Assert(CommandListHandle.GraphicsCommandList());
		D3D12_VIEWPORT vp;
		vp.Width = (float)SizeX;
		vp.Height = (float)SizeY;
		vp.MinDepth = 0;
		vp.MaxDepth = 1;
		vp.TopLeftX = (float)TopLeftX;
		vp.TopLeftY = (float)TopLeftY;
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
				TransitionResource(RenderTargetRHI->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, false);
				D3D12TargetViews.emplace_back(RenderTargetRHI->GetRTV());
			}
		}
		D3D12_CPU_DESCRIPTOR_HANDLE DSV{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
		if (DepthRHI)
		{
			DSV = DepthRHI->GetDSV();
			TransitionResource(DepthRHI->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, false);
		}
		if (D3D12TargetViews.empty() && DSV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_NULL)
			return;
		CommandListHandle.FlushResourceBarriers();
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
		if (RenderTarget)
		{
			TransitionResource(TexRHI->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
			CommandListHandle.GraphicsCommandList()->ClearRenderTargetView(TexRHI->GetRTV(), &Color.R, 0, nullptr);
		}
		if (DepthRHI)
		{
			TransitionResource(DepthRHI->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
			CommandListHandle.GraphicsCommandList()->ClearDepthStencilView(DepthRHI->GetRTV(), D3D12_CLEAR_FLAG_DEPTH, Depth, Stencil, 0, nullptr);
		}
	}

	void D3D12CommandContext::RHIBeing()
	{
		Assert(CommandAllocator);
		if (CommandAllocator)
			CommandListHandle.Reset(*CommandAllocator);
		
	}

	void D3D12CommandContext::RHISetShaderSampler(EShaderFrequency ShaderType, uint32_t SamplerIndex, std::shared_ptr<RHISamplerState> NewState)
	{
		if (!StateCache)
		{
			return;
		}
		auto SampleState = RHIResourceCast(NewState.get());
		if (!SampleState)
		{
			return;
		}

		switch (ShaderType)
		{
		case SF_Vertex:
			StateCache->SetSamplerState<SF_Vertex>(SampleState->GetSampleDesc(), SamplerIndex);
			break;
		case SF_Hull:
			StateCache->SetSamplerState<SF_Hull>(SampleState->GetSampleDesc(), SamplerIndex);
			break;
		case SF_Domain:
			StateCache->SetSamplerState<SF_Domain>(SampleState->GetSampleDesc(), SamplerIndex);
			break;
		case SF_Pixel:
			StateCache->SetSamplerState<SF_Pixel>(SampleState->GetSampleDesc(), SamplerIndex);
			break;
		case SF_Geometry:
			StateCache->SetSamplerState<SF_Geometry>(SampleState->GetSampleDesc(), SamplerIndex);
			break;
		case SF_Compute:
			StateCache->SetSamplerState<SF_Compute>(SampleState->GetSampleDesc(), SamplerIndex);
			break;
		default:
			Assert(false);
			break;
		}
	}

	void D3D12CommandContext::RHISetRasterizerState(std::shared_ptr<RHIRasterizerState> NewStateRHI)
	{
		if (!StateCache)
		{
			return;
		}
		auto RasterizerState = RHIResourceCast(NewStateRHI.get());
		if (!RasterizerState)
		{
			return;
		}
		StateCache->SetRasterizerState(RasterizerState->GetRasterizerDesc());
	}

	void D3D12CommandContext::RHISetBlendState(std::shared_ptr<RHIBlendState> NewState, const core::FLinearColor& BlendFactor)
	{
		if (!StateCache)
		{
			return;
		}
		auto BlendState = RHIResourceCast(NewState.get());
		if (BlendState)
		{
			StateCache->SetBlendState(BlendState->GetBlendDesc());
		}
		StateCache->SetBlendFactor(&BlendFactor.R);
	}

	void D3D12CommandContext::RHISetBlendFactor(const core::FLinearColor& BlendFactor)
	{
		if (!StateCache)
		{
			return;
		}
		StateCache->SetBlendFactor(&BlendFactor.R);
	}

	void D3D12CommandContext::RHISetDepthStencilState(std::shared_ptr< RHIDepthStencilState> NewState, uint32_t StencilRef)
	{
		if (!StateCache)
		{
			return;
		}
		auto DepthStencilState = RHIResourceCast(NewState.get());
		if (DepthStencilState)
		{
			StateCache->SetDepthStencilState(DepthStencilState->GetDepthStencilDesc());
		}
		StateCache->SetStencilRef(StencilRef);
	}

	void D3D12CommandContext::RHISetStencilRef(uint32_t StencilRef)
	{
		if (!StateCache)
		{
			return;
		}
		StateCache->SetStencilRef(StencilRef);
	}

	void D3D12CommandContext::RHISetGraphicsPipelineState(const GraphicsPipelineStateInitializer& Initializer)
	{
		if (!StateCache)
		{
			return;
		}

		if (Initializer.BlendState)
		{
			RHISetBlendState(Initializer.BlendState, core::FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		}
		if (Initializer.DepthStencilState)
		{
			RHISetDepthStencilState(Initializer.DepthStencilState, 0);
		}

		if (Initializer.RasterizerState)
		{
			RHISetRasterizerState(Initializer.RasterizerState);
		}

		if (Initializer.VertexShader)
		{
			FD3D12VertexShader* VertexShaderRHI = RHIResourceCast(Initializer.VertexShader.get());
			//StateCache.SetInputLayout(VertexShaderRHI->GetNativeInputLayout());
			if (VertexShaderRHI)
			{
				StateCache->SetVertexShader(std::static_pointer_cast<FD3D12VertexShader>(Initializer.VertexShader));
			}
			
		}
		else
		{
			StateCache->SetVertexShader(nullptr);
		}

		if (Initializer.PixelShader)
		{
			FD3D12PixelShader* PixelShaderRHI = RHIResourceCast(Initializer.PixelShader.get());
			//StateCache.SetPixelShader(PixelShaderRHI->GetNativePixelShader());
		}
		else
		{
			//StateCache->SetPixelShader(nullptr);
		}
	}

	D3D12CommandListHandle D3D12CommandContext::FlushCommands(bool WaitForCompletion /*= false*/)
	{
		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		const bool bHasDoneWork = HasDoneWork() ;
		const bool bOpenNewCmdList = WaitForCompletion || bHasDoneWork;

		// Only submit a command list if it does meaningful work or the flush is expected to wait for completion.
		if (bOpenNewCmdList)
		{
			// Close the current command list
			CloseCommandList();

			// Just submit the current command list
			CommandListHandle.Execute(WaitForCompletion);

			// Get a new command list to replace the one we submitted for execution. 
			// Restore the state from the previous command list.
			OpenCommandList();
		}

		return CommandListHandle;
	}

	FD3D12CommandListManager& D3D12CommandContext::GetCommandListManager()
	{
		return bIsAsyncComputeContext ? GetParentDevice()->GetCommandListManager(ED3D12CommandQueueType::Async) : GetParentDevice()->GetCommandListManager(ED3D12CommandQueueType::Default);
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
			CommandListHandle.AddTransitionBarrier(Resource, Resource->GetResourceState().GetSubresourceState(0), NewState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			if (Flush)
				CommandListHandle.FlushResourceBarriers();
			Resource->GetResourceState().SetResourceState(NewState);
		}

	}

	void D3D12CommandContext::InitializeTexture(FD3D12Resource* Dest, UINT NumSubResources, D3D12_SUBRESOURCE_DATA SubData[])
	{
		Assert(Dest);
		ConditionalObtainCommandAllocator();
		D3D12CommandAllocator* TempCommandAllocator = CommandAllocatorManager.ObtainCommandAllocator();
		// Get a new command list
		auto CommandList = GetCommandListManager().ObtainCommandList(*TempCommandAllocator);
		CommandList.SetCurrentOwningContext(this);

		size_t UploadBufferSize = (size_t)GetRequiredIntermediateSize(Dest->GetResource(), 0, NumSubResources);
		FAllocation Allocation = CpuLinearAllocator.Allocate(UploadBufferSize);
		UpdateSubresources(CommandList.GraphicsCommandList(), Dest->GetResource(), Allocation.D3d12Resource, 0, 0, NumSubResources, SubData);
		CommandList.AddTransitionBarrier(Dest, D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

		CommandList.Close();
		CommandList.Execute(true);
		CommandAllocatorManager.ReleaseCommandAllocator(TempCommandAllocator);
	}

	LinearAllocator& D3D12CommandContext::GetLinerAllocator(ELinearAllocatorType type)
	{
		Assert(type == CpuWritable || type == GpuExclusive);
		if (type == CpuWritable)
		{
			return CpuLinearAllocator;
		}
		else
		{
			return GpuLinearAllocator;
		}
	}

}
