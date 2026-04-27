#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12ReourceTraits.h"
#include "D3D12/D3D12CommandList.h"
#include "D3D12/D3D12StateCache.h"
#include "D3D12/D3D12GenerateMips.h"
#include "D3D12/D3D12StateCache.h"
#include "D3D12/D3D12RHI.h"
#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12CreateStats.h"
#include "D3D12/D3D12SubmitStats.h"
#include "D3D12/D3D12PresentStats.h"
#include "D3D12/D3D12MemoryMonitor.h"
#include "D3D12/D3D12RuntimeStatsMonitor.h"
#include "pix.h"
#include "core/logger.h"
#include "win/high_precision_tick.h"
#include "core/commandline.h"
#include <windows.h>
#include <heapapi.h>

#include "../../../ThirdParty/DirectXTex/DXTexStats.h"

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
		CurrentStateCache = {};
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
		if (!CurrentStateCache)
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
		// Defer barrier flush until Clear/Draw/Dispatch/Close — OMSetRenderTargets does not consume the RT/DS contents.
		CommandListHandle.GraphicsCommandList()->OMSetRenderTargets((uint32_t)D3D12TargetViews.size(), D3D12TargetViews.data(), FALSE, DepthRHI ? &DSV : nullptr);
		CurrentStateCache->SetRenderTargetFormats(Targets, Depth);	
	}

	void D3D12CommandContext::SetRenderTarget(std::shared_ptr<RHITexture2D> Tex, std::shared_ptr< RHITexture2D> Depth)
	{
		std::vector<std::shared_ptr<RHITexture2D>> Targets{ Tex };
		SetRenderTarget(Targets, Depth);
	}

	void D3D12CommandContext::SetRenderTarget(std::shared_ptr< RHIRenderTarget> RenderTarget, int32_t IndexMip /*= 0*/)
	{
		if (!CurrentStateCache)
			return;
		Assert(CommandListHandle.GraphicsCommandList());
		auto RenderTargetRHI = RHIResourceCast(RenderTarget.get());
		if (RenderTargetRHI && RenderTargetRHI->GetMipRTV(IndexMip).ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
		{
			TransitionSubResource(RenderTargetRHI->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, IndexMip, false);
			if(RenderTargetRHI->GetDepthResource())
				TransitionResource(RenderTargetRHI->GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, false);
			D3D12_CPU_DESCRIPTOR_HANDLE RTV = RenderTargetRHI->GetMipRTV(IndexMip);
			D3D12_CPU_DESCRIPTOR_HANDLE DSV = RenderTargetRHI->GetDSV();
			CommandListHandle.GraphicsCommandList()->OMSetRenderTargets((uint32_t)1, &RTV, FALSE, 
													RenderTargetRHI->GetDepthResource() ? &DSV : nullptr);
			CurrentStateCache->SetRenderTargetFormat(RenderTargetRHI);
		}

	}

	void D3D12CommandContext::SetRenderTarget(std::shared_ptr<RHITextureCube> TextureCube, int32_t IndexView, int32_t IndexMip)
	{
		if (!CurrentStateCache)
			return;
		Assert(CommandListHandle.GraphicsCommandList());
		auto TextureCubeRHI = RHIResourceCast(TextureCube.get());
		if (TextureCubeRHI && TextureCubeRHI->GetRTV(IndexView,IndexMip).ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
		{
			TransitionResource(TextureCubeRHI->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, false);
			if (TextureCubeRHI->GetDepthResource())
				TransitionResource(TextureCubeRHI->GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, false);
			D3D12_CPU_DESCRIPTOR_HANDLE RTV = TextureCubeRHI->GetRTV(IndexView, IndexMip);
			D3D12_CPU_DESCRIPTOR_HANDLE DSV = TextureCubeRHI->GetDSV();
			CommandListHandle.GraphicsCommandList()->OMSetRenderTargets((uint32_t)1, &RTV, FALSE, 
				TextureCubeRHI->GetDepthResource() ? &DSV : nullptr);
			CurrentStateCache->SetRenderTargetFormat(TextureCubeRHI);
		}
	}

	void D3D12CommandContext::SetRenderTarget(D3D12TextureCube* TextureCube, int32_t IndexView, int32_t IndexMip)
	{
		if (!CurrentStateCache)
			return;
		Assert(CommandListHandle.GraphicsCommandList());
		if (TextureCube && TextureCube->GetRTV(IndexView, IndexMip).ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE RTV = TextureCube->GetRTV(IndexView, IndexMip);
			D3D12_CPU_DESCRIPTOR_HANDLE DSV = TextureCube->GetDSV();
			CommandListHandle.GraphicsCommandList()->OMSetRenderTargets((uint32_t)1, &RTV, FALSE,
				TextureCube->GetDepthResource() ? &DSV : nullptr);
			CurrentStateCache->SetRenderTargetFormat(TextureCube);
		}
	}

	void D3D12CommandContext::Clear(std::shared_ptr<RHITexture2D> RenderTarget, std::shared_ptr<RHITexture2D> DepthTarget,
									const core::FLinearColor& Color, float Depth /*= 1.0f*/, uint8_t Stencil /*= 0*/)
	{
		Assert(CommandListHandle.GraphicsCommandList());
		auto TargetRHI = RHIResourceCast(RenderTarget.get());
		auto DepthRHI = RHIResourceCast(DepthTarget.get());
		// Ensure resources are in the correct state for clear operations.
		if (TargetRHI)
			TransitionResource(TargetRHI->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, false);
		if (DepthRHI)
			TransitionResource(DepthRHI->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, false);
		CommandListHandle.FlushResourceBarriers();
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
				TransitionResource(TargetRHI->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, false);
		}
		auto DepthRHI = RHIResourceCast(DepthTarget.get());
		if (DepthRHI)
			TransitionResource(DepthRHI->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, false);
		CommandListHandle.FlushResourceBarriers();
		for (std::shared_ptr<RHITexture2D> Target: Targets)
		{
			auto TargetRHI = RHIResourceCast(Target.get());
			if (TargetRHI)
				CommandListHandle.GraphicsCommandList()->ClearRenderTargetView(TargetRHI->GetRTV(), &Color.R, 0, nullptr);
		}
		if (DepthRHI)
		{
			CommandListHandle.GraphicsCommandList()->ClearDepthStencilView(DepthRHI->GetDSV(), D3D12_CLEAR_FLAG_DEPTH, Depth, Stencil, 0, nullptr);
		}
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
			// Only transition the specific face/mip we are clearing.
			const uint32_t SubresourceIndex = TextureCubeRHI->GetSubresourceIndex(Face, Mip);
			TransitionSubResource(TextureCubeRHI->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, SubresourceIndex, false);
			if (TextureCubeRHI->GetDepthResource())
				TransitionResource(TextureCubeRHI->GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, false);
			CommandListHandle.FlushResourceBarriers();
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
		TransitionResource(RenderTargetRHI->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, false);
		if (RenderTargetRHI->GetDepthResource())
			TransitionResource(RenderTargetRHI->GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, false);
		CommandListHandle.FlushResourceBarriers();

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
		if (CurrentStateCache)
			CurrentStateCache->InvalidateDescriptorHeapBindingsForFreshCommandList();
		for (const auto& KV : StateCacheMap)
		{
			if (KV.second)
				KV.second->InvalidateDescriptorHeapBindingsForFreshCommandList();
		}
	}

	void D3D12CommandContext::RHIEndDrawing()
	{
		win32::RecordPresentFrameForFpsLog();

		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		if (!Device)
			return;

		const std::shared_ptr<FD3D12Adapter> Adapter = TryGetParentAdapter();
		// Not in MS MiniEngine hot path; optional leak/WC diagnosis (hurts frame time).
		if (Adapter && core::CommandLine::Get().GetName("d3d12forceidle"))
			Adapter->BlockUntilIdle();

		if (Adapter)
		{
			D3D12RuntimeStatsMonitor::TickOncePerSecond(*this, Adapter, Device);
			D3D12MemoryMonitor::TickOncePerSecond(Adapter, Device);
		}
	}

	void D3D12CommandContext::RHISetShaderSampler(EShaderFrequency ShaderType, uint32_t SamplerIndex, std::shared_ptr<RHISamplerState> NewState)
	{
		if (!CurrentStateCache)
			return;
		auto SampleState = RHIResourceCast(NewState.get());
		if (!SampleState)
			return;

		switch (ShaderType)
		{
		case SF_Vertex:
			CurrentStateCache->SetSamplerState<SF_Vertex>(SampleState->GetSampleDesc(), SamplerIndex);
			break;
		case SF_Hull:
			CurrentStateCache->SetSamplerState<SF_Hull>(SampleState->GetSampleDesc(), SamplerIndex);
			break;
		case SF_Domain:
			CurrentStateCache->SetSamplerState<SF_Domain>(SampleState->GetSampleDesc(), SamplerIndex);
			break;
		case SF_Pixel:
			CurrentStateCache->SetSamplerState<SF_Pixel>(SampleState->GetSampleDesc(), SamplerIndex);
			break;
		case SF_Geometry:
			CurrentStateCache->SetSamplerState<SF_Geometry>(SampleState->GetSampleDesc(), SamplerIndex);
			break;
		case SF_Compute:
			CurrentStateCache->SetSamplerState<SF_Compute>(SampleState->GetSampleDesc(), SamplerIndex);
			break;
		default:
			Assert(false);
			break;
		}
	}

	void D3D12CommandContext::RHISetRasterizerState(std::shared_ptr<RHIRasterizerState> NewStateRHI)
	{
		if (!CurrentStateCache)
			return;
		auto RasterizerState = RHIResourceCast(NewStateRHI.get());
		if (!RasterizerState)
			return;
		CurrentStateCache->SetRasterizerState(RasterizerState->GetRasterizerDesc());
	}

	void D3D12CommandContext::RHISetBlendState(std::shared_ptr<RHIBlendState> NewState, const core::FLinearColor& BlendFactor)
	{
		if (!CurrentStateCache)
			return;
		auto BlendState = RHIResourceCast(NewState.get());
		if (BlendState)
		{
			CurrentStateCache->SetBlendState(BlendState->GetBlendDesc());
		}
		CurrentStateCache->SetBlendFactor(&BlendFactor.R);
	}

	void D3D12CommandContext::RHISetBlendFactor(const core::FLinearColor& BlendFactor)
	{
		if (!CurrentStateCache)
			return;
		CurrentStateCache->SetBlendFactor(&BlendFactor.R);
	}

	void D3D12CommandContext::RHISetDepthStencilState(std::shared_ptr< RHIDepthStencilState> NewState, uint32_t StencilRef)
	{
		if (!CurrentStateCache)
			return;
		auto DepthStencilState = RHIResourceCast(NewState.get());
		if (DepthStencilState)
			CurrentStateCache->SetDepthStencilState(DepthStencilState->GetDepthStencilDesc());
		CurrentStateCache->SetStencilRef(StencilRef);
	}

	void D3D12CommandContext::RHISetStencilRef(uint32_t StencilRef)
	{
		if (!CurrentStateCache)
			return;
		CurrentStateCache->SetStencilRef(StencilRef);
	}

	void D3D12CommandContext::RHISetGraphicsPipelineState(const GraphicsPipelineStateInitializer& Initializer)
	{
		auto d3d12VertexShader = std::static_pointer_cast<FD3D12VertexShader>(Initializer.VertexShader);
		auto d3d12PixelShader = std::static_pointer_cast<FD3D12PixelShader>(Initializer.PixelShader);

		std::string key; 
		if (d3d12VertexShader)
			key = std::to_string(d3d12VertexShader->Hash);
		if (d3d12PixelShader)
			key += "_" + std::to_string(d3d12PixelShader->Hash);
		
		auto itFind = StateCacheMap.find(key);
		if (itFind != StateCacheMap.end())
		{
			CurrentStateCache = itFind->second;
		}
		else
		{
			CurrentStateCache = std::make_shared<FD3D12StateCache>(GetParentAdapter()->GetDevice(), this->shared_from_this());
			StateCacheMap.emplace(std::make_pair(key, CurrentStateCache));
		}

		if (Initializer.BlendState)
			RHISetBlendState(Initializer.BlendState, core::FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		if (Initializer.DepthStencilState)
			RHISetDepthStencilState(Initializer.DepthStencilState, 0);
		if (Initializer.RasterizerState)
			RHISetRasterizerState(Initializer.RasterizerState);

		if (Initializer.VertexShader)
			CurrentStateCache->SetVertexShader(d3d12VertexShader);
		else
			CurrentStateCache->SetVertexShader(nullptr);

		if (Initializer.PixelShader)
			CurrentStateCache->SetPixelShader(d3d12PixelShader);
		else
			CurrentStateCache->SetPixelShader(nullptr);
		CurrentStateCache->SetComputeShader(nullptr);
		CurrentStateCache->SetPrimitiveTopology(GetD3D12PrimitiveType(Initializer.PrimitiveType, false));
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
		if (!CurrentStateCache)
			return;

		D3D12Texture2D* Texture2D = RHIResourceCast(Texture2DRHI.get());
		if (Texture2D)
		{
			if(ShaderType == SF_Pixel)
				TransitionResource(Texture2D->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, false);
			if(ShaderType == SF_Compute)
				TransitionResource(Texture2D->GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
			CurrentStateCache->SetShaderResourceView(ShaderType, TextureIndex, std::static_pointer_cast<D3D12Texture2D>(Texture2DRHI));
		}
	}

	void D3D12CommandContext::RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<RHITextureCube> TextureCubeRHI)
	{
		if (!CurrentStateCache)
			return;
		D3D12TextureCube* TextureCube = RHIResourceCast(TextureCubeRHI.get());
		if (TextureCube)
		{
			if (ShaderType == SF_Pixel)
				TransitionResource(TextureCube->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, false);
			if (ShaderType == SF_Compute)
				TransitionResource(TextureCube->GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
			CurrentStateCache->SetShaderResourceView(ShaderType, TextureIndex, -1,std::static_pointer_cast<D3D12TextureCube>(TextureCubeRHI));
		}
	}

	void D3D12CommandContext::RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, int32_t Mip, std::shared_ptr<RHITextureCube> TextureCubeRHI)
	{
		if (!CurrentStateCache)
			return;
		D3D12TextureCube* TextureCube = RHIResourceCast(TextureCubeRHI.get());
		if (TextureCube)
		{
			if (ShaderType == SF_Pixel)
				TransitionSubResource(TextureCube->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, Mip, false);
			if (ShaderType == SF_Compute)
				TransitionSubResource(TextureCube->GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, Mip, false);
			CurrentStateCache->SetShaderResourceView(ShaderType, TextureIndex, Mip, std::static_pointer_cast<D3D12TextureCube>(TextureCubeRHI));
		}
	}

	void D3D12CommandContext::RHISetUAVParameter(uint32_t UAVIndex, std::shared_ptr<RHIUnorderedAccessView> UAV)
	{
		if (!CurrentStateCache)
			return;
		auto TexRHI = std::static_pointer_cast<D3D12Texture2D>(UAV->GetTexture2D());
		TransitionResource(TexRHI->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
		CurrentStateCache->SetUAV(UAVIndex, TexRHI);
	}

	void D3D12CommandContext::RHISetShaderUniformBuffer(EShaderFrequency ShaderType, uint32_t BufferIndex, std::shared_ptr<RHIUniformBuffer> UniformBufferRHI)
	{
		if (!CurrentStateCache)
			return;

		D3D12UniformBuffer* UniformBuffer = RHIResourceCast(UniformBufferRHI.get());
		if (UniformBuffer)
			CurrentStateCache->SetDynamicConstantBuffer(ShaderType,BufferIndex, std::static_pointer_cast<D3D12UniformBuffer>(UniformBufferRHI));
	}

	void D3D12CommandContext::DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI)
	{
		if (!CurrentStateCache)
			return;

		D3D12VertexBffer* VertexBuffer = RHIResourceCast(VertexBufferRHI.get());
		D3D12IndexBuffer* IndexBuffer = RHIResourceCast(IndexBufferRHI.get());
		if (!VertexBuffer || !IndexBuffer)
			return;

		CurrentStateCache->SetVertexBuffer(CommandListHandle, 0, VertexBuffer->VertexBufferView());
		CurrentStateCache->SetIndexBuffer(CommandListHandle, IndexBuffer->IndexBufferView());
		if (!CurrentStateCache->ApplyGraphicState(CommandListHandle))
			return;
		CommandListHandle.FlushResourceBarriers();
		CommandListHandle->DrawIndexedInstanced(IndexBuffer->GetIndexCount(),1, 0, 0,0);
		++numDraws;
	}

	void D3D12CommandContext::DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI)
	{
		if (!CurrentStateCache)
			return;

		D3D12VertexBffer* VertexBuffer = RHIResourceCast(VertexBufferRHI.get());
		if (!VertexBuffer)
			return;

		CurrentStateCache->SetVertexBuffer(CommandListHandle, 0, VertexBuffer->VertexBufferView());
		D3D12_INDEX_BUFFER_VIEW IndexView{};
		IndexView.Format = DXGI_FORMAT_UNKNOWN;
		CurrentStateCache->SetIndexBuffer(CommandListHandle, IndexView);
		if (!CurrentStateCache->ApplyGraphicState(CommandListHandle))
			return;
		CommandListHandle.FlushResourceBarriers();
		CommandListHandle->DrawInstanced(VertexBuffer->GetCount(),1,0,0);
		++numDraws;
	}

	void D3D12CommandContext::DrawPrimitive(const std::array<std::shared_ptr<RHIVertexBuffer>, VT_Max>& VertexBufferArrayRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI)
	{
		if (!CurrentStateCache)
			return;

		int32_t StreamIndex = 0;
		for (const auto& BufferRHI : VertexBufferArrayRHI)
		{
			if (BufferRHI)
			{
				D3D12VertexBffer* VertexBuffer = RHIResourceCast(BufferRHI.get());
				CurrentStateCache->SetVertexBuffer(CommandListHandle, StreamIndex++, VertexBuffer->VertexBufferView());
			}
		}
		D3D12IndexBuffer* IndexBuffer = RHIResourceCast(IndexBufferRHI.get());
		if (!IndexBuffer)
			return;
		CurrentStateCache->SetIndexBuffer(CommandListHandle, IndexBuffer->IndexBufferView());
		if (!CurrentStateCache->ApplyGraphicState(CommandListHandle))
			return;
		CommandListHandle.FlushResourceBarriers();
		CommandListHandle->DrawIndexedInstanced(IndexBuffer->GetIndexCount(),1,0,0,0);
		++numDraws;
	}

	void D3D12CommandContext::Draw(uint32_t VertexCount, uint32_t VertexStartOffset /*= 0*/)
	{
		if (!CurrentStateCache)
			return;
		if (!CurrentStateCache->ApplyGraphicState(CommandListHandle))
			return;
		CommandListHandle.FlushResourceBarriers();
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
		auto computeShader = std::static_pointer_cast<FD3D12ComputeShader>(Initializer.ComputeShader);
		if (!computeShader)
			return;
		std::string key = std::to_string(computeShader->Hash);

		auto itFind = StateCacheMap.find(key);
		if (itFind != StateCacheMap.end())
		{
			CurrentStateCache = itFind->second;
		}
		else
		{
			CurrentStateCache = std::make_shared<FD3D12StateCache>(GetParentAdapter()->GetDevice(), this->shared_from_this());
			StateCacheMap.emplace(std::make_pair(key, CurrentStateCache));
		}
		CurrentStateCache->SetVertexShader(nullptr);
		CurrentStateCache->SetPixelShader(nullptr);
		CurrentStateCache->SetComputeShader(computeShader);

	}

	void D3D12CommandContext::RHIDispatchComputeShader(uint32_t ThreadGroupCountX, uint32_t ThreadGroupCountY, uint32_t ThreadGroupCountZ)
	{
		if (!CurrentStateCache)
			return;
		if (!CurrentStateCache->ApplyComputeState(CommandListHandle))
			return;
		CommandListHandle.FlushResourceBarriers();
		CommandListHandle->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
		++numDispatches;
	}

	void D3D12CommandContext::RHICopyResource(std::shared_ptr< RHITexture2D> DstTex, std::shared_ptr< RHITexture2D> SrcTex)
	{
		auto D3D12Src = RHIResourceCast(SrcTex.get());
		auto D3D12Dst = RHIResourceCast(DstTex.get());
		if (!D3D12Src || !D3D12Dst)
			return;
		
		auto SrcOldState = D3D12Src->GetResource()->GetResourceState().GetSubresourceState(0);
		auto DstOldState = D3D12Dst->GetResource()->GetResourceState().GetSubresourceState(0);
		TransitionSubResource(D3D12Src->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, 0, false);
		TransitionSubResource(D3D12Dst->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, 0, false);
		// Only flush barriers; avoid submitting mid-frame.
		CommandListHandle.FlushResourceBarriers();
		CommandListHandle->CopyResource(D3D12Dst->GetResource()->GetResource(), D3D12Src->GetResource()->GetResource());
		
		TransitionSubResource(D3D12Dst->GetResource(), DstOldState, 0, false);
		TransitionSubResource(D3D12Src->GetResource(), SrcOldState, 0, false);
		// Restore barriers are ordered after Copy in the batch; defer flush to the next GPU boundary
		// (Draw/Clear/Dispatch/Close) to avoid an extra ResourceBarrier split per copy.
	}

	void D3D12CommandContext::RHICopyResource2D(std::shared_ptr< RHITexture2D> DstTex, std::shared_ptr< RHITexture2D> SrcTex, core::vec4u rect)
	{
		auto D3D12Src = RHIResourceCast(SrcTex.get());
		auto D3D12Dst = RHIResourceCast(DstTex.get());
		if (!D3D12Src || !D3D12Dst)
			return;
		auto SrcOldState = D3D12Src->GetResource()->GetResourceState().GetSubresourceState(0);
		auto DstOldState = D3D12Dst->GetResource()->GetResourceState().GetSubresourceState(0);
		TransitionSubResource(D3D12Src->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, 0, false);
		TransitionSubResource(D3D12Dst->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, 0, false);
		// Only flush barriers; avoid submitting mid-frame.
		CommandListHandle.FlushResourceBarriers();

		D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
		srcLocation.pResource = D3D12Src->GetResource()->GetResource();
		srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		srcLocation.SubresourceIndex = 0; 

		D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
		dstLocation.pResource = D3D12Dst->GetResource()->GetResource();
		dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstLocation.SubresourceIndex = 0; 

		D3D12_BOX srcBox = {};
		srcBox.left = rect.left();
		srcBox.top = rect.top();
		srcBox.front = 0;  
		srcBox.right = rect.right();
		srcBox.bottom = rect.bottom(); 
		srcBox.back = 1;

		CommandListHandle->CopyTextureRegion(
			&dstLocation,
			0, 0, 0,
			&srcLocation,
			&srcBox
		);

		TransitionSubResource(D3D12Src->GetResource(), SrcOldState, 0, false);
		TransitionSubResource(D3D12Dst->GetResource(), DstOldState, 0, false);
	}

	void D3D12CommandContext::FlushCommands(bool WaitForCompletion /*= false*/)
	{
		(void)FlushCommandsGetFence(WaitForCompletion);
	}

	uint64_t D3D12CommandContext::FlushCommandsGetFence(bool WaitForCompletion /*= false*/)
	{
		// Submit path: always close, execute, and run per-submit cleanup (linear allocators, dynamic heaps)
		// via the ExecuteAndClear fence callback. Skipping submit for heuristics breaks fence-tied retirement.
		CloseCommandList();
		const uint64_t SignaledFenceValue = CommandListHandle.ExecuteAndClear(WaitForCompletion);
		OpenCommandList();
		return SignaledFenceValue;
	}

	uint64_t D3D12CommandContext::FlushCommandsGetFence_NoReopen(bool WaitForCompletion /*= false*/)
	{
		if (!CommandListHandle)
			return 0;
		// Shutdown/idle-wait path: we only need to submit outstanding work and optionally wait.
		// Re-opening a fresh command list here forces a Reset() on the driver hot path and can crash
		// during teardown (observed in nvwgf2umx on some systems).
		CloseCommandList();
		const uint64_t SignaledFenceValue = CommandListHandle.ExecuteAndClear(WaitForCompletion);
		return SignaledFenceValue;
	}

	void D3D12CommandContext::RHITransitionResource(std::shared_ptr< RHITexture2D> Tex, int32_t NewState, bool Flush /*= false*/)
	{
		auto TexRHI = RHIResourceCast(Tex.get());
		if (TexRHI)
			TransitionResource(TexRHI->GetResource(), (D3D12_RESOURCE_STATES)NewState, Flush);
	}

	void D3D12CommandContext::BeginUserMark(const char* name)
	{
		ID3D12GraphicsCommandList* commandBuffer = GetCurrentCommandListHandle().GraphicsCommandList();
		if(commandBuffer)
			PIXBeginEvent(commandBuffer, 0, name);
	}


	void D3D12CommandContext::EndUserMark()
	{
		ID3D12GraphicsCommandList* commandBuffer = GetCurrentCommandListHandle().GraphicsCommandList();
		if (commandBuffer)
			PIXEndEvent(commandBuffer);
	}

	FD3D12CommandListManager& D3D12CommandContext::GetCommandListManager()
	{
		return bIsAsyncComputeContext ? GetParentDevice()->GetCommandListManager(ED3D12CommandQueueType::Async) : GetParentDevice()->GetCommandListManager(ED3D12CommandQueueType::Default);
	}

	void D3D12CommandContext::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, win32::com_ptr<ID3D12DescriptorHeap> HeapPtr)
	{
		if (!CurrentStateCache)
			return;
		CurrentStateCache->SetDescriptorHeap(CommandListHandle, Type, HeapPtr);
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

		if (CurrentStateCache)
			CurrentStateCache->InvalidateDescriptorHeapBindingsForFreshCommandList();
		for (const auto& KV : StateCacheMap)
		{
			if (KV.second)
				KV.second->InvalidateDescriptorHeapBindingsForFreshCommandList();
		}
		numDraws = 0;
		numDispatches = 0;
		numClears = 0;
		numBarriers = 0;
		numCopies = 0;
		otherWorkCounter = 0;
	}

	void D3D12CommandContext::CloseCommandList()
	{
		if (!CommandListHandle)
			return;
		CommandListHandle.Close();
	}

	void D3D12CommandContext::TransitionResource(FD3D12Resource* Resource, D3D12_RESOURCE_STATES NewState, bool Flush /*= false*/)
	{
		// MiniEngine-style optimization:
		// Use a single ALL_SUBRESOURCES barrier whenever all subresources share the same old state.
		// Fall back to per-subresource barriers only when states are mixed.
		const uint16_t SubresourceCount = Resource->GetSubresourceCount();
		if (SubresourceCount == 0)
			return;

		const D3D12_RESOURCE_STATES Old0 = Resource->GetResourceState().GetSubresourceState(0);

		bool bAllSameOld = true;
		for (uint16_t s = 1; s < SubresourceCount; ++s)
		{
			if (Resource->GetResourceState().GetSubresourceState(s) != Old0)
			{
				bAllSameOld = false;
				break;
			}
		}

		bool bAnyTransition = false;
		if (bAllSameOld)
		{
			if (Old0 != NewState)
			{
				CommandListHandle.AddTransitionBarrier(Resource, Old0, NewState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
				Resource->GetResourceState().SetResourceState(NewState);
				bAnyTransition = true;
			}
		}
		else
		{
			for (uint16_t Subresource = 0; Subresource < SubresourceCount; ++Subresource)
			{
				const D3D12_RESOURCE_STATES OldState = Resource->GetResourceState().GetSubresourceState(Subresource);
				if (OldState != NewState)
				{
					CommandListHandle.AddTransitionBarrier(Resource, OldState, NewState, Subresource);
					Resource->GetResourceState().SetSubresourceState(Subresource, NewState);
					bAnyTransition = true;
				}
			}
		}

		if (bAnyTransition && Flush)
			CommandListHandle.FlushResourceBarriers();
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
		Render::D3D12CallStats::AddUploadBytes((uint64_t)UploadBufferSize);
		FAllocation Allocation = CommandList.GetLinerAllocator(ELinearAllocatorType::CpuWritable).Allocate(UploadBufferSize);
		UpdateSubresources(CommandList.GraphicsCommandList(), Dest->GetResource(), Allocation.Resource->GetResource(), 0, 0, NumSubResources, SubData);
		Render::D3D12CallStats::AddCopyBytes((uint64_t)UploadBufferSize);
		CommandList.AddTransitionBarrier(Dest, D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		Dest->GetResourceState().SetResourceState(D3D12_RESOURCE_STATE_GENERIC_READ);
		CommandList.Close();
		(void)CommandList.ExecuteAndClear(true);
		CommandAllocatorManager.ReleaseCommandAllocator(TempCommandAllocator);
	}

	void D3D12CommandContext::InitializeBuffer(FD3D12Resource* Dest, const void* Data, uint32_t NumBytes, size_t Offset /*= 0*/)
	{
		Assert(Dest);
		D3D12CommandAllocator* TempCommandAllocator = CommandAllocatorManager.ObtainCommandAllocator();
		// Get a new command list
		auto CommandList = GetCommandListManager().ObtainCommandList(*TempCommandAllocator);
		CommandList.SetCurrentOwningContext(this);

		Render::D3D12CallStats::AddUploadBytes((uint64_t)NumBytes);
		FAllocation Allocation = CommandList.GetLinerAllocator(ELinearAllocatorType::CpuWritable).Allocate(NumBytes);
		memcpy(Allocation.CPU, Data, NumBytes);

		D3D12_RESOURCE_STATES OldState = Dest->GetResourceState().GetSubresourceState(0);
		if (OldState != D3D12_RESOURCE_STATE_COPY_DEST)
		{
			CommandList.AddTransitionBarrier(Dest, OldState, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			CommandList.FlushResourceBarriers();
		}

		Render::D3D12CallStats::AddCopyBytes((uint64_t)NumBytes);
		CommandList->CopyBufferRegion(Dest->GetResource(), Offset, Allocation.Resource->GetResource(), 0, NumBytes);
		CommandList.AddTransitionBarrier(Dest, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		Dest->GetResourceState().SetResourceState(D3D12_RESOURCE_STATE_GENERIC_READ);
		CommandList.Close();
		(void)CommandList.ExecuteAndClear(true);
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
	}

	void D3D12CommandContext::Destroy()
	{
		StateCacheMap.clear();
		CurrentStateCache = {};
		D3D12GenerateMips = {};
		// Release the current command list handle before tearing down managers/allocators.
		CommandListHandle = {};
		if(CommandAllocator)
			CommandAllocatorManager.ReleaseCommandAllocator(CommandAllocator);
		CommandAllocator = nullptr;
	}

	void D3D12CommandContext::ClearState()
	{
		if (CurrentStateCache)
			CurrentStateCache->ClearState();
	}

	void D3D12CommandContext::CleanupUsedHeaps(uint64_t FenceValue, ED3D12CommandQueueType QueueType)
	{
		for (auto& StateCache : StateCacheMap)
		{
			if (StateCache.second)
				StateCache.second->CleanupUsedHeaps(FenceValue, QueueType);
		}

		// CurrentStateCache is normally one of the map entries; calling twice is harmless and avoids missing cleanup if map/state ever diverge.
		if (CurrentStateCache)
			CurrentStateCache->CleanupUsedHeaps(FenceValue, QueueType);
	}

	std::shared_ptr<FD3D12StateCache> D3D12CommandContext::GetD3D12StateCache() const
	{
		return CurrentStateCache;
	}

	D3D12CommandListHandle& D3D12CommandContext::GetCurrentCommandListHandle()
	{
		return CommandListHandle;
	}

}
