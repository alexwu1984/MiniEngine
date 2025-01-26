#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12ReourceTraits.h"
#include "D3D12/D3D12CommandList.h"
#include "D3D12/D3D12StateCache.h"
#include "D3D12/D3D12GenerateMips.h"

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
		CommandAllocator(nullptr),
		CommandAllocatorManager(InParent, InIsAsyncComputeContext ? D3D12_COMMAND_LIST_TYPE_COMPUTE : D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		
	}

	D3D12CommandContext::~D3D12CommandContext()
	{
		StateCache = {};
		
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

		CD3DX12_RECT ScissorRect(TopLeftX, TopLeftY, TopLeftX + SizeX, TopLeftY + SizeY);
		CommandListHandle.GraphicsCommandList()->RSSetScissorRects(1, &ScissorRect);
	}

	void D3D12CommandContext::SetRenderTarget(const std::vector<std::shared_ptr<RHITexture2D>>& Targets, std::shared_ptr< RHITexture2D> Depth)
	{
		if (!StateCache)
			return;
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
		CommandListHandle.FlushResourceBarriers();
		CommandListHandle.GraphicsCommandList()->OMSetRenderTargets((uint32_t)D3D12TargetViews.size(), D3D12TargetViews.data(), FALSE, DepthRHI ? &DSV : nullptr);
		StateCache->SetRenderTargetFormats(Targets, Depth);	
	}

	void D3D12CommandContext::SetRenderTarget(std::shared_ptr<RHITexture2D> Tex, std::shared_ptr< RHITexture2D> Depth)
	{
		std::vector<std::shared_ptr<RHITexture2D>> Targets{ Tex };
		SetRenderTarget(Targets, Depth);
	}

	void D3D12CommandContext::SetRenderTarget(std::shared_ptr< RHIRenderTarget> RenderTarget, int32_t IndexMip /*= 0*/)
	{
		if (!StateCache)
			return;
		Assert(CommandListHandle.GraphicsCommandList());
		auto RenderTargetRHI = RHIResourceCast(RenderTarget.get());
		if (RenderTargetRHI && RenderTargetRHI->GetMipRTV(IndexMip).ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
		{
			TransitionSubResource(RenderTargetRHI->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, IndexMip, false);
			if(RenderTargetRHI->GetDepthResource())
				TransitionResource(RenderTargetRHI->GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, false);
			CommandListHandle.FlushResourceBarriers();
			D3D12_CPU_DESCRIPTOR_HANDLE RTV = RenderTargetRHI->GetMipRTV(IndexMip);
			D3D12_CPU_DESCRIPTOR_HANDLE DSV = RenderTargetRHI->GetDSV();
			CommandListHandle.GraphicsCommandList()->OMSetRenderTargets((uint32_t)1, &RTV, FALSE, 
													RenderTargetRHI->GetDepthResource() ? &DSV : nullptr);
			StateCache->SetRenderTargetFormat(RenderTargetRHI);
		}

	}

	void D3D12CommandContext::SetRenderTarget(std::shared_ptr<RHITextureCube> TextureCube, int32_t IndexView, int32_t IndexMip)
	{
		if (!StateCache)
			return;
		Assert(CommandListHandle.GraphicsCommandList());
		auto TextureCubeRHI = RHIResourceCast(TextureCube.get());
		if (TextureCubeRHI && TextureCubeRHI->GetRTV(IndexView,IndexMip).ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
		{
			TransitionResource(TextureCubeRHI->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, false);
			if (TextureCubeRHI->GetDepthResource())
				TransitionResource(TextureCubeRHI->GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, false);
			CommandListHandle.FlushResourceBarriers();
			D3D12_CPU_DESCRIPTOR_HANDLE RTV = TextureCubeRHI->GetRTV(IndexView, IndexMip);
			D3D12_CPU_DESCRIPTOR_HANDLE DSV = TextureCubeRHI->GetDSV();
			CommandListHandle.GraphicsCommandList()->OMSetRenderTargets((uint32_t)1, &RTV, FALSE, 
				TextureCubeRHI->GetDepthResource() ? &DSV : nullptr);
			StateCache->SetRenderTargetFormat(TextureCubeRHI);
		}
	}

	void D3D12CommandContext::SetRenderTarget(D3D12TextureCube* TextureCube, int32_t IndexView, int32_t IndexMip)
	{
		if (!StateCache)
			return;
		Assert(CommandListHandle.GraphicsCommandList());
		if (TextureCube && TextureCube->GetRTV(IndexView, IndexMip).ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE RTV = TextureCube->GetRTV(IndexView, IndexMip);
			D3D12_CPU_DESCRIPTOR_HANDLE DSV = TextureCube->GetDSV();
			CommandListHandle.GraphicsCommandList()->OMSetRenderTargets((uint32_t)1, &RTV, FALSE,
				TextureCube->GetDepthResource() ? &DSV : nullptr);
			StateCache->SetRenderTargetFormat(TextureCube);
		}
	}

	void D3D12CommandContext::Clear(std::shared_ptr<RHITexture2D> RenderTarget, std::shared_ptr<RHITexture2D> DepthTarget,
									const core::FLinearColor& Color, float Depth /*= 1.0f*/, uint8_t Stencil /*= 0*/)
	{
		Assert(CommandListHandle.GraphicsCommandList());
		auto TargetRHI = RHIResourceCast(RenderTarget.get());
		auto DepthRHI = RHIResourceCast(DepthTarget.get());
		if (TargetRHI)
			CommandListHandle.GraphicsCommandList()->ClearRenderTargetView(TargetRHI->GetRTV(), &Color.R, 0, nullptr);
		if (DepthRHI)
			CommandListHandle.GraphicsCommandList()->ClearDepthStencilView(DepthRHI->GetDSV(), D3D12_CLEAR_FLAG_DEPTH, Depth, Stencil, 0, nullptr);
		++numClears;
	}

	void D3D12CommandContext::Clear(std::vector<std::shared_ptr<RHITexture2D>> Targets, std::shared_ptr<RHITexture2D> DepthTarget, 
									const core::FLinearColor& Color, float Depth /*= 1.0f*/, uint8_t Stencil /*= 0*/)
	{
		Assert(CommandListHandle.GraphicsCommandList());
		for (std::shared_ptr<RHITexture2D> Target: Targets)
		{
			auto TargetRHI = RHIResourceCast(Target.get());
			if (TargetRHI)
				CommandListHandle.GraphicsCommandList()->ClearRenderTargetView(TargetRHI->GetRTV(), &Color.R, 0, nullptr);
		}
		auto DepthRHI = RHIResourceCast(DepthTarget.get());
		if (DepthRHI)
			CommandListHandle.GraphicsCommandList()->ClearDepthStencilView(DepthRHI->GetDSV(), D3D12_CLEAR_FLAG_DEPTH, Depth, Stencil, 0, nullptr);
		++numClears;
	}

	void D3D12CommandContext::Clear(std::shared_ptr< RHITextureCube> TextureCube, int32_t Face, int32_t Mip, const core::FLinearColor& Color, float Depth /*= 1.0f*/, uint8_t Stencil /*= 0*/)
	{
		Assert(CommandListHandle.GraphicsCommandList());
		auto TextureCubeRHI = RHIResourceCast(TextureCube.get());
		if (!TextureCubeRHI)
			return;
		if (TextureCubeRHI->GetRTV(Face, Mip).ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE RTV = TextureCubeRHI->GetRTV(Face, Mip);
			CommandListHandle.GraphicsCommandList()->ClearRenderTargetView(RTV, &Color.R, 0, nullptr);
			D3D12_CPU_DESCRIPTOR_HANDLE DSV = TextureCubeRHI->GetDSV();
			if(DSV.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
				CommandListHandle.GraphicsCommandList()->ClearDepthStencilView(DSV, D3D12_CLEAR_FLAG_DEPTH, Depth, Stencil, 0, nullptr);
		}
		++numClears;
	}

	void D3D12CommandContext::Clear(std::shared_ptr< RHIRenderTarget> RenderTarget, const core::FLinearColor& Color, float Depth /*= 1.0f*/, uint8_t Stencil /*= 0*/)
	{
		Assert(CommandListHandle.GraphicsCommandList());
		auto RenderTargetRHI = RHIResourceCast(RenderTarget.get());
		if (!RenderTargetRHI)
			return;
		if (RenderTargetRHI->GetRTV().ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE RTV = RenderTargetRHI->GetRTV();
			CommandListHandle.GraphicsCommandList()->ClearRenderTargetView(RTV, &Color.R, 0, nullptr);
		}

		if (RenderTargetRHI->GetDSV().ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE DSV = RenderTargetRHI->GetDSV();
			CommandListHandle.GraphicsCommandList()->ClearDepthStencilView(DSV, D3D12_CLEAR_FLAG_DEPTH, Depth, Stencil, 0, nullptr);
		}
		++numClears;
	}

	void D3D12CommandContext::RHIBeing()
	{
		Assert(CommandAllocator);
		if (CommandAllocator)
			CommandListHandle.Reset(*CommandAllocator);
		
	}

	void D3D12CommandContext::RHIEndDrawing()
	{
		if (!StateCache)
			return;
		StateCache->ClearState();
	}

	void D3D12CommandContext::RHISetShaderSampler(EShaderFrequency ShaderType, uint32_t SamplerIndex, std::shared_ptr<RHISamplerState> NewState)
	{
		if (!StateCache)
			return;
		auto SampleState = RHIResourceCast(NewState.get());
		if (!SampleState)
			return;

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
			return;
		auto RasterizerState = RHIResourceCast(NewStateRHI.get());
		if (!RasterizerState)
			return;
		StateCache->SetRasterizerState(RasterizerState->GetRasterizerDesc());
	}

	void D3D12CommandContext::RHISetBlendState(std::shared_ptr<RHIBlendState> NewState, const core::FLinearColor& BlendFactor)
	{
		if (!StateCache)
			return;
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
			return;
		StateCache->SetBlendFactor(&BlendFactor.R);
	}

	void D3D12CommandContext::RHISetDepthStencilState(std::shared_ptr< RHIDepthStencilState> NewState, uint32_t StencilRef)
	{
		if (!StateCache)
			return;
		auto DepthStencilState = RHIResourceCast(NewState.get());
		if (DepthStencilState)
			StateCache->SetDepthStencilState(DepthStencilState->GetDepthStencilDesc());
		StateCache->SetStencilRef(StencilRef);
	}

	void D3D12CommandContext::RHISetStencilRef(uint32_t StencilRef)
	{
		if (!StateCache)
			return;
		StateCache->SetStencilRef(StencilRef);
	}

	void D3D12CommandContext::RHISetGraphicsPipelineState(const GraphicsPipelineStateInitializer& Initializer)
	{
		if (!StateCache)
			return;

		StateCache->ClearRenderState();

		if (Initializer.BlendState)
			RHISetBlendState(Initializer.BlendState, core::FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		if (Initializer.DepthStencilState)
			RHISetDepthStencilState(Initializer.DepthStencilState, 0);
		if (Initializer.RasterizerState)
			RHISetRasterizerState(Initializer.RasterizerState);

		if (Initializer.VertexShader)
			StateCache->SetVertexShader(std::static_pointer_cast<FD3D12VertexShader>(Initializer.VertexShader));
		else
			StateCache->SetVertexShader(nullptr);

		if (Initializer.PixelShader)
			StateCache->SetPixelShader(std::static_pointer_cast<FD3D12PixelShader>(Initializer.PixelShader));
		else
			StateCache->SetPixelShader(nullptr);

		StateCache->SetPrimitiveTopology(GetD3D12PrimitiveType(Initializer.PrimitiveType,false));
	}

	void D3D12CommandContext::RHIUpdateUniformBuffer(std::shared_ptr<RHIUniformBuffer> UniformBufferRHI, const void* Contents)
	{
		if (!Contents)
			return;
		D3D12UniformBuffer* UniformBuffer = RHIResourceCast(UniformBufferRHI.get());
		if (UniformBuffer)
			UniformBuffer->UpdateUniformBuffer(Contents);
	}

	void D3D12CommandContext::RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<RHITexture2D> Texture2DRHI)
	{
		if (!StateCache)
			return;

		D3D12Texture2D* Texture2D = RHIResourceCast(Texture2DRHI.get());
		if (Texture2D)
		{
			if(ShaderType == SF_Pixel)
				TransitionResource(Texture2D->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
			if(ShaderType == SF_Compute)
				GetParentDevice()->GetDefaultCommandContext()->TransitionResource(Texture2D->GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);
			StateCache->SetShaderResourceView(ShaderType, TextureIndex, std::static_pointer_cast<D3D12Texture2D>(Texture2DRHI));
		}
	}

	void D3D12CommandContext::RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<RHITextureCube> TextureCubeRHI)
	{
		if (!StateCache)
			return;
		D3D12TextureCube* TextureCube = RHIResourceCast(TextureCubeRHI.get());
		if (TextureCube)
		{
			if (ShaderType == SF_Pixel)
				TransitionResource(TextureCube->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
			if (ShaderType == SF_Compute)
				GetParentDevice()->GetDefaultCommandContext()->TransitionResource(TextureCube->GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);
			StateCache->SetShaderResourceView(ShaderType, TextureIndex, -1,std::static_pointer_cast<D3D12TextureCube>(TextureCubeRHI));
		}
	}

	void D3D12CommandContext::RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, int32_t Mip, std::shared_ptr<RHITextureCube> TextureCubeRHI)
	{
		if (!StateCache)
			return;
		D3D12TextureCube* TextureCube = RHIResourceCast(TextureCubeRHI.get());
		if (TextureCube)
		{
			if (ShaderType == SF_Pixel)
				TransitionSubResource(TextureCube->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, Mip, true);
			if (ShaderType == SF_Compute)
				GetParentDevice()->GetDefaultCommandContext()->TransitionSubResource(TextureCube->GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, Mip, true);
			StateCache->SetShaderResourceView(ShaderType, TextureIndex, Mip, std::static_pointer_cast<D3D12TextureCube>(TextureCubeRHI));
		}
	}

	void D3D12CommandContext::RHISetUAVParameter(uint32_t UAVIndex, std::shared_ptr<RHIUnorderedAccessView> UAV)
	{
		if (!StateCache)
			return;
		auto TexRHI = std::static_pointer_cast<D3D12Texture2D>(UAV->GetTexture2D());
		TransitionResource(TexRHI->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
		StateCache->SetUAV(UAVIndex, TexRHI);
	}

	void D3D12CommandContext::RHISetShaderUniformBuffer(EShaderFrequency ShaderType, uint32_t BufferIndex, std::shared_ptr<RHIUniformBuffer> UniformBufferRHI)
	{
		if (!StateCache)
			return;

		D3D12UniformBuffer* UniformBuffer = RHIResourceCast(UniformBufferRHI.get());
		if (UniformBuffer)
			StateCache->SetDynamicConstantBuffer(ShaderType,BufferIndex, std::static_pointer_cast<D3D12UniformBuffer>(UniformBufferRHI));
	}

	void D3D12CommandContext::DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI)
	{
		if (!StateCache)
			return;

		D3D12VertexBffer* VertexBuffer = RHIResourceCast(VertexBufferRHI.get());
		D3D12IndexBuffer* IndexBuffer = RHIResourceCast(IndexBufferRHI.get());
		if (!VertexBuffer || !IndexBuffer)
			return;

		StateCache->SetVertexBuffer(CommandListHandle, 0, VertexBuffer->VertexBufferView());
		StateCache->SetIndexBuffer(CommandListHandle, IndexBuffer->IndexBufferView());
		if (!StateCache->ApplyGraphicState(CommandListHandle))
			return;
		CommandListHandle->DrawIndexedInstanced(IndexBuffer->GetIndexCount(),1, 0, 0,0);
		++numDraws;
	}

	void D3D12CommandContext::DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI)
	{
		if (!StateCache)
			return;

		D3D12VertexBffer* VertexBuffer = RHIResourceCast(VertexBufferRHI.get());
		if (!VertexBuffer)
			return;

		StateCache->SetVertexBuffer(CommandListHandle, 0, VertexBuffer->VertexBufferView());
		D3D12_INDEX_BUFFER_VIEW IndexView{};
		IndexView.Format = DXGI_FORMAT_UNKNOWN;
		StateCache->SetIndexBuffer(CommandListHandle, IndexView);
		if (!StateCache->ApplyGraphicState(CommandListHandle))
			return;
		CommandListHandle->DrawInstanced(VertexBuffer->GetCount(),1,0,0);
		++numDraws;
	}

	void D3D12CommandContext::DrawPrimitive(const std::array<std::shared_ptr<RHIVertexBuffer>, VT_Max>& VertexBufferArrayRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI)
	{
		if (!StateCache)
			return;

		int32_t StreamIndex = 0;
		for (const auto& BufferRHI : VertexBufferArrayRHI)
		{
			if (BufferRHI)
			{
				D3D12VertexBffer* VertexBuffer = RHIResourceCast(BufferRHI.get());
				StateCache->SetVertexBuffer(CommandListHandle, StreamIndex++, VertexBuffer->VertexBufferView());
			}
		}
		D3D12IndexBuffer* IndexBuffer = RHIResourceCast(IndexBufferRHI.get());
		if (!IndexBuffer)
			return;
		StateCache->SetIndexBuffer(CommandListHandle, IndexBuffer->IndexBufferView());
		if (!StateCache->ApplyGraphicState(CommandListHandle))
			return;
		CommandListHandle->DrawIndexedInstanced(IndexBuffer->GetIndexCount(),1,0,0,0);
		++numDraws;
	}

	void D3D12CommandContext::Draw(uint32_t VertexCount, uint32_t VertexStartOffset /*= 0*/)
	{
		if (!StateCache)
			return;
		if (!StateCache->ApplyGraphicState(CommandListHandle))
			return;
		CommandListHandle->DrawInstanced(VertexCount, 1, VertexStartOffset, 0);
		++numDraws;
	}

	void D3D12CommandContext::GenerateMips(std::shared_ptr<RHITextureCube> TextureCubeRHI)
	{
		if (!D3D12GenerateMips)
		{
			D3D12GenerateMips = std::make_shared<FD3D12GenerateMips>(GetParentAdapter());
			D3D12GenerateMips->InitResource();
		}
		D3D12GenerateMips->GenerateForCube(TextureCubeRHI, this);
	}

	void D3D12CommandContext::RHISetComputePipelineState(const ComputePipelineStateInitializer& Initializer)
	{
		if (!StateCache)
			return;
		StateCache->ClearComputeState();

		if (Initializer.ComputeShader)
			StateCache->SetComputeShader(std::static_pointer_cast<FD3D12ComputeShader>(Initializer.ComputeShader));
		else
			StateCache->SetComputeShader(nullptr);
	}

	void D3D12CommandContext::RHIDispatchComputeShader(uint32_t ThreadGroupCountX, uint32_t ThreadGroupCountY, uint32_t ThreadGroupCountZ)
	{
		if (!StateCache)
			return;
		if (!StateCache->ApplyComputeState(CommandListHandle))
			return;
		CommandListHandle->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
		++numDispatches;
	}

	void D3D12CommandContext::FlushCommands(bool WaitForCompletion /*= false*/)
	{
		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		const bool bHasDoneWork = HasDoneWork();
		const bool bOpenNewCmdList = WaitForCompletion || bHasDoneWork;

		// Only submit a command list if it does meaningful work or the flush is expected to wait for completion.
		if (bOpenNewCmdList)
		{
			// Close the current command list
			CloseCommandList();

			// Just submit the current command list
			CommandListHandle.ExecuteAndClear(WaitForCompletion);

			// Get a new command list to replace the one we submitted for execution. 
			// Restore the state from the previous command list.
			OpenCommandList();
		}
	}

	FD3D12CommandListManager& D3D12CommandContext::GetCommandListManager()
	{
		return bIsAsyncComputeContext ? GetParentDevice()->GetCommandListManager(ED3D12CommandQueueType::Async) : GetParentDevice()->GetCommandListManager(ED3D12CommandQueueType::Default);
	}

	void D3D12CommandContext::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, win32::com_ptr<ID3D12DescriptorHeap> HeapPtr)
	{
		if (!StateCache)
			return;
		StateCache->SetDescriptorHeap(CommandListHandle, Type, HeapPtr);
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

	std::shared_ptr<RenderCore::FD3D12Device> D3D12CommandContext::GetParentDevice() const
	{
		return GetParentAdapter()->GetDevice();
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
		for (uint16_t i = 0; i < Resource->GetSubresourceCount(); ++i)
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

	void D3D12CommandContext::TransitionSubResource(FD3D12Resource* Resource, D3D12_RESOURCE_STATES NewState, uint32_t Subresource, bool Flush)
	{
		Assert(Subresource < Resource->GetSubresourceCount());
		D3D12_RESOURCE_STATES OldState = Resource->GetResourceState().GetSubresourceState(Subresource);
		if (OldState != NewState)
		{
			CommandListHandle.AddTransitionBarrier(Resource, OldState, NewState, Subresource);
			if (Flush)
				CommandListHandle.FlushResourceBarriers();
			Resource->GetResourceState().SetSubresourceState(Subresource, NewState);
		}
	}

	void D3D12CommandContext::InitializeTexture(FD3D12Resource* Dest, UINT NumSubResources, D3D12_SUBRESOURCE_DATA SubData[])
	{
		Assert(Dest);
		D3D12CommandAllocator* TempCommandAllocator = CommandAllocatorManager.ObtainCommandAllocator();
		// Get a new command list
		auto CommandList = GetCommandListManager().ObtainCommandList(*TempCommandAllocator);
		CommandList.SetCurrentOwningContext(this);

		size_t UploadBufferSize = (size_t)GetRequiredIntermediateSize(Dest->GetResource(), 0, NumSubResources);
		FAllocation Allocation = CommandList.GetLinerAllocator(ELinearAllocatorType::CpuWritable).Allocate(UploadBufferSize);
		UpdateSubresources(CommandList.GraphicsCommandList(), Dest->GetResource(), Allocation.Resource->GetResource(), 0, 0, NumSubResources, SubData);
		CommandList.AddTransitionBarrier(Dest, D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		Dest->GetResourceState().SetResourceState(D3D12_RESOURCE_STATE_GENERIC_READ);
		CommandList.Close();
		CommandList.ExecuteAndClear(true);
		CommandAllocatorManager.ReleaseCommandAllocator(TempCommandAllocator);
	}

	void D3D12CommandContext::InitializeBuffer(FD3D12Resource* Dest, const void* Data, uint32_t NumBytes, size_t Offset /*= 0*/)
	{
		Assert(Dest);
		D3D12CommandAllocator* TempCommandAllocator = CommandAllocatorManager.ObtainCommandAllocator();
		// Get a new command list
		auto CommandList = GetCommandListManager().ObtainCommandList(*TempCommandAllocator);
		CommandList.SetCurrentOwningContext(this);

		FAllocation Allocation = CommandList.GetLinerAllocator(ELinearAllocatorType::CpuWritable).Allocate(NumBytes);
		memcpy(Allocation.CPU, Data, NumBytes);

		D3D12_RESOURCE_STATES OldState = Dest->GetResourceState().GetSubresourceState(0);
		if (OldState != D3D12_RESOURCE_STATE_COPY_DEST)
		{
			CommandList.AddTransitionBarrier(Dest, OldState, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			CommandList.FlushResourceBarriers();
		}

		CommandList->CopyBufferRegion(Dest->GetResource(), Offset, Allocation.Resource->GetResource(), 0, NumBytes);
		CommandList.AddTransitionBarrier(Dest, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		Dest->GetResourceState().SetResourceState(D3D12_RESOURCE_STATE_GENERIC_READ);
		CommandList.Close();
		CommandList.ExecuteAndClear(true);
		CommandAllocatorManager.ReleaseCommandAllocator(TempCommandAllocator);
	}

	LinearAllocator& D3D12CommandContext::GetLinerAllocator(ELinearAllocatorType type)
	{
		Assert(type == CpuWritable || type == GpuExclusive);
		Assert(CommandListHandle != nullptr);
		return CommandListHandle.GetLinerAllocator(type);
	}

	void D3D12CommandContext::Initialize(void)
	{
		StateCache = std::make_shared<FD3D12StateCache>(GetParentAdapter()->GetDevice(),this->shared_from_this());
	}

	void D3D12CommandContext::Destroy()
	{
		StateCache = {};
		D3D12GenerateMips = {};
		if(CommandAllocator)
			CommandAllocatorManager.ReleaseCommandAllocator(CommandAllocator);
		CommandAllocator = nullptr;
	}

	void D3D12CommandContext::ClearState()
	{
		if (StateCache)
			StateCache->ClearState();
	}

	FD3D12StateCache& D3D12CommandContext::GetD3D12StateCache() const
	{
		Assert(StateCache.get());
		return *StateCache;
	}

	D3D12CommandListHandle& D3D12CommandContext::GetCurrentCommandListHandle()
	{
		return CommandListHandle;
	}

}
