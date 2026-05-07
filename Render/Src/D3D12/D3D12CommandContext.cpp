#include "D3D12/D3D12CommandContext.h"
#include "RHI/RDGResourceAccess.h"
#include "D3D12/D3D12Util.h"
#include "RHI/RHIThreadPolicy.h"
#include "D3D12/D3D12RHIRecording.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12GpuTimestampRing.h"
#include "D3D12/D3D12ReourceTraits.h"
#include "D3D12/D3D12CommandList.h"
#include "D3D12/D3D12StateCache.h"
#include "D3D12/D3D12GenerateMips.h"
#include "D3D12/D3D12RHI.h"
#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12CreateStats.h"
#include "D3D12/D3D12SubmitStats.h"
#include "D3D12/D3D12PresentStats.h"
#include "D3D12/D3D12MemoryMonitor.h"
#include "D3D12/D3D12RuntimeStatsMonitor.h"
#include "RHI/RHI.h"
#include "RHI/RHIDefinitions.h"
#include "core/logger.h"
#include "win/high_precision_tick.h"
#include "core/commandline.h"
#include "DirectXTex/DXTexStats.h"
#include <pix.h>

namespace RenderCore
{
	namespace
	{
		bool IsDepthStencilPixelFormat(EPixelFormat PF)
		{
			switch (PF)
			{
			case PF_DepthStencil:
			case PF_ShadowDepth:
			case PF_D24:
				return true;
			default:
				return false;
			}
		}

		D3D12_RESOURCE_STATES ShaderReadableStateForTexture2DSample(const D3D12Texture2D* Tex2D, bool bAsyncCompute)
		{
			if (!Tex2D)
				return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			if (IsDepthStencilPixelFormat(Tex2D->GetPixelFormat()))
				return D3D12_RESOURCE_STATE_DEPTH_READ;
			return bAsyncCompute ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}

		// D32_FLOAT_S8X24 (etc.) exposes multiple planes; depth SRV reads plane 0 only. Transitioning every subresource to
		// DEPTH_READ corrupts the stencil plane state and can wedge GPU validation / drivers while the CPU blocks on fences/Present.
		bool ShouldTransitionDepthPlaneOnlyForShaderSample(const D3D12Texture2D* Tex2D, FD3D12Resource* Res)
		{
			return Tex2D && Res && IsDepthStencilPixelFormat(Tex2D->GetPixelFormat()) && Res->GetPlaneCount() > 1;
		}
	} // namespace
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
		++otherWorkCounter;
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
				// GetRTV() is mip0 only (D3D12Texture2D::CreateDerivedViews). Whole-resource RT transitions
				// collapse tracking and break mixed mip RT/PSRV (#527) when other mips stay shader-visible.
				FD3D12Resource* const Res = RenderTargetRHI->GetResource();
				if (Res && Res->RequiresResourceStateTracking())
					TransitionSubResource(Res, D3D12_RESOURCE_STATE_RENDER_TARGET, 0, false);
				D3D12TargetViews.emplace_back(RenderTargetRHI->GetRTV());
			}
		}
		D3D12_CPU_DESCRIPTOR_HANDLE DSV{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };
		bool bBindDepth = false;
		if (DepthRHI)
		{
			DSV = DepthRHI->GetDSV();
			bBindDepth = (DSV.ptr != 0u);
			if (bBindDepth)
				TransitionResource(DepthRHI->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, false);
		}
		// Defer barrier flush until Clear/Draw/Dispatch/Close — OMSetRenderTargets does not consume the RT/DS contents.
		// If the depth texture exists but DSV was never created, binding &DSV with ptr==0 behaves like no DSV while
		// PSDesc would still pick D32 — D3D12 ERROR #615 (DEPTH_STENCIL_FORMAT_MISMATCH_PIPELINE_STATE).
		CommandListHandle.GraphicsCommandList()->OMSetRenderTargets((uint32_t)D3D12TargetViews.size(), D3D12TargetViews.data(), FALSE, bBindDepth ? &DSV : nullptr);
		CurrentStateCache->SetRenderTargetFormats(Targets, bBindDepth ? Depth : nullptr);
		++otherWorkCounter;
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
			const bool bBindDsv = RenderTargetRHI->GetDepthResource() && RenderTargetRHI->GetDSV().ptr != 0u;
			if (bBindDsv)
				TransitionResource(RenderTargetRHI->GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, false);
			D3D12_CPU_DESCRIPTOR_HANDLE RTV = RenderTargetRHI->GetMipRTV(IndexMip);
			D3D12_CPU_DESCRIPTOR_HANDLE DSV = RenderTargetRHI->GetDSV();
			CommandListHandle.GraphicsCommandList()->OMSetRenderTargets((uint32_t)1, &RTV, FALSE, bBindDsv ? &DSV : nullptr);
			CurrentStateCache->SetRenderTargetFormat(RenderTargetRHI);
			++otherWorkCounter;
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
			FD3D12Resource* const Res = TextureCubeRHI->GetResource();
			if (Res && Res->RequiresResourceStateTracking())
			{
				const uint32_t SubIdx = TextureCubeRHI->GetSubresourceIndex(IndexView, IndexMip);
				TransitionSubResource(Res, D3D12_RESOURCE_STATE_RENDER_TARGET, SubIdx, false);
			}
			const bool bBindDsvCube = TextureCubeRHI->GetDepthResource() && TextureCubeRHI->GetDSV().ptr != 0u;
			if (bBindDsvCube)
				TransitionResource(TextureCubeRHI->GetDepthResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, false);
			D3D12_CPU_DESCRIPTOR_HANDLE RTV = TextureCubeRHI->GetRTV(IndexView, IndexMip);
			D3D12_CPU_DESCRIPTOR_HANDLE DSV = TextureCubeRHI->GetDSV();
			CommandListHandle.GraphicsCommandList()->OMSetRenderTargets((uint32_t)1, &RTV, FALSE, bBindDsvCube ? &DSV : nullptr);
			CurrentStateCache->SetRenderTargetFormat(TextureCubeRHI);
			++otherWorkCounter;
		}
	}

	void D3D12CommandContext::SetRenderTarget(D3D12TextureCube* TextureCube, int32_t IndexView, int32_t IndexMip)
	{
		if (!CurrentStateCache)
			return;
		Assert(CommandListHandle.GraphicsCommandList());
		if (TextureCube && TextureCube->GetRTV(IndexView, IndexMip).ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
		{
			FD3D12Resource* const Res = TextureCube->GetResource();
			if (Res && Res->RequiresResourceStateTracking())
			{
				const uint32_t SubIdx = TextureCube->GetSubresourceIndex(IndexView, IndexMip);
				TransitionSubResource(Res, D3D12_RESOURCE_STATE_RENDER_TARGET, SubIdx, false);
			}
			D3D12_CPU_DESCRIPTOR_HANDLE RTV = TextureCube->GetRTV(IndexView, IndexMip);
			D3D12_CPU_DESCRIPTOR_HANDLE DSV = TextureCube->GetDSV();
			const bool bBindDsvRaw = TextureCube->GetDepthResource() && TextureCube->GetDSV().ptr != 0u;
			CommandListHandle.GraphicsCommandList()->OMSetRenderTargets((uint32_t)1, &RTV, FALSE,
				bBindDsvRaw ? &DSV : nullptr);
			CurrentStateCache->SetRenderTargetFormat(TextureCube);
			++otherWorkCounter;
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
		{
			FD3D12Resource* const Res = TargetRHI->GetResource();
			if (Res && Res->RequiresResourceStateTracking())
				TransitionSubResource(Res, D3D12_RESOURCE_STATE_RENDER_TARGET, 0, false);
		}
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
			{
				FD3D12Resource* const Res = TargetRHI->GetResource();
				if (Res && Res->RequiresResourceStateTracking())
					TransitionSubResource(Res, D3D12_RESOURCE_STATE_RENDER_TARGET, 0, false);
			}
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
		if (FD3D12Resource* const Res = RenderTargetRHI->GetResource())
		{
			if (Res->RequiresResourceStateTracking())
				TransitionSubResource(Res, D3D12_RESOURCE_STATE_RENDER_TARGET, 0, false);
		}
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

		if (Adapter && D3D12RHI_ShouldEnableMemMon())
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

		EnsureStateCache();
		if (!CurrentStateCache)
			return;

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
		if (!Texture2D)
		{
			CurrentStateCache->SetShaderResourceView(ShaderType, TextureIndex, std::shared_ptr<D3D12Texture2D>{});
			return;
		}

		if (ShaderType == SF_Pixel || ShaderType == SF_Compute)
		{
			const bool bCompute = (ShaderType == SF_Compute);
			const D3D12_RESOURCE_STATES TargetState = ShaderReadableStateForTexture2DSample(Texture2D, bCompute);
			FD3D12Resource* const Res = Texture2D->GetResource();
			if (Res && Res->RequiresResourceStateTracking())
			{
				// Depth/stencil SRV reads plane 0 only (#527-style); color textures use one whole-resource
				// transition when all mips share the same target (fewer ResourceBarrier calls vs per-sub loop).
				if (ShouldTransitionDepthPlaneOnlyForShaderSample(Texture2D, Res))
					TransitionSubResource(Res, TargetState, 0, false);
				else
					TransitionResource(Res, TargetState, false);
			}
		}
		CurrentStateCache->SetShaderResourceView(ShaderType, TextureIndex, std::static_pointer_cast<D3D12Texture2D>(Texture2DRHI));
	}

	void D3D12CommandContext::RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<RHITextureCube> TextureCubeRHI)
	{
		if (!CurrentStateCache)
			return;
		D3D12TextureCube* TextureCube = RHIResourceCast(TextureCubeRHI.get());
		if (TextureCube)
		{
			if (ShaderType == SF_Pixel || ShaderType == SF_Compute)
			{
				const D3D12_RESOURCE_STATES TargetState = (ShaderType == SF_Compute)
					? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
					: D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				FD3D12Resource* const Res = TextureCube->GetResource();
				if (Res && Res->RequiresResourceStateTracking())
					TransitionResource(Res, TargetState, false);
			}
			CurrentStateCache->SetShaderResourceView(ShaderType, TextureIndex, -1, std::static_pointer_cast<D3D12TextureCube>(TextureCubeRHI));
		}
	}

	void D3D12CommandContext::RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, int32_t Mip, std::shared_ptr<RHITextureCube> TextureCubeRHI)
	{
		if (!CurrentStateCache)
			return;
		D3D12TextureCube* TextureCube = RHIResourceCast(TextureCubeRHI.get());
		if (TextureCube)
		{
			// Subresources are laid out as Face * NumMips + MipSlice (see D3D12TextureCube::GetSubresourceIndex).
			// A cube SRV at one mip touches all six faces; transition each slice that mip level uses.
			if (ShaderType == SF_Pixel || ShaderType == SF_Compute)
			{
				const D3D12_RESOURCE_STATES TargetState = (ShaderType == SF_Compute)
					? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
					: D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				FD3D12Resource* const Res = TextureCube->GetResource();
				if (Res && Res->RequiresResourceStateTracking())
				{
					for (int Face = 0; Face < 6; ++Face)
					{
						const uint32_t SubIdx = TextureCube->GetSubresourceIndex(Face, Mip);
						TransitionSubResource(Res, TargetState, SubIdx, false);
					}
				}
			}
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

	void D3D12CommandContext::RHISetGraphicsRoot32BitConstants(uint32_t RootParameterIndex, uint32_t Num32BitValues, const void* SrcData, uint32_t DestOffsetIn32BitValues)
	{
		if (!CommandListHandle)
			return;
		CommandListHandle->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValues, SrcData, DestOffsetIn32BitValues);
		++otherWorkCounter;
	}

	void D3D12CommandContext::DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI)
	{
		DrawPrimitiveInstanced(VertexBufferRHI, IndexBufferRHI, 1u, 0u);
	}

	void D3D12CommandContext::DrawPrimitiveInstanced(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI, uint32_t InstanceCount, uint32_t StartInstanceLocation)
	{
		if (!CurrentStateCache || InstanceCount == 0)
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
		CommandListHandle->DrawIndexedInstanced(IndexBuffer->GetIndexCount(), InstanceCount, 0, 0, StartInstanceLocation);
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
		static constexpr D3D12_VERTEX_BUFFER_VIEW kNullVBV{};
		for (uint32_t s = static_cast<uint32_t>(StreamIndex); s < 32u; ++s)
			CurrentStateCache->SetVertexBuffer(CommandListHandle, s, kNullVBV);
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

	void D3D12CommandContext::DrawPrimitiveInstanced(const std::array<std::shared_ptr<RHIVertexBuffer>, VT_Max>& VertexBufferArrayRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI, uint32_t InstanceCount,
													 uint32_t StartInstanceLocation)
	{
		if (!CurrentStateCache || InstanceCount == 0)
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
		static constexpr D3D12_VERTEX_BUFFER_VIEW kNullVBV{};
		for (uint32_t s = static_cast<uint32_t>(StreamIndex); s < 32u; ++s)
			CurrentStateCache->SetVertexBuffer(CommandListHandle, s, kNullVBV);
		D3D12IndexBuffer* IndexBuffer = RHIResourceCast(IndexBufferRHI.get());
		if (!IndexBuffer)
			return;
		CurrentStateCache->SetIndexBuffer(CommandListHandle, IndexBuffer->IndexBufferView());
		if (!CurrentStateCache->ApplyGraphicState(CommandListHandle))
			return;
		CommandListHandle.FlushResourceBarriers();
		CommandListHandle->DrawIndexedInstanced(IndexBuffer->GetIndexCount(), InstanceCount, 0, 0, StartInstanceLocation);
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
		EnsureStateCache();
		if (!CurrentStateCache)
			return;
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
		
		// Use per-command-list state: global is only advanced after submit (see CommitTrackedResourceStateToGlobal).
		const D3D12_RESOURCE_STATES SrcOldState = CommandListHandle.GetResourceState(D3D12Src->GetResource()).GetSubresourceState(0);
		const D3D12_RESOURCE_STATES DstOldState = CommandListHandle.GetResourceState(D3D12Dst->GetResource()).GetSubresourceState(0);
		TransitionSubResource(D3D12Src->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, 0, false);
		TransitionSubResource(D3D12Dst->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, 0, false);
		// Only flush barriers; avoid submitting mid-frame.
		CommandListHandle.FlushResourceBarriers();
		CommandListHandle->CopyResource(D3D12Dst->GetResource()->GetResource(), D3D12Src->GetResource()->GetResource());
		++numCopies;

		// Never emit transitions into TBD/CORRUPT — invalid StateAfter breaks validation and can wedge the GPU/debug runtime.
		// Omit restore when unknown; subsequent binds (RTV/SRV) transition from COPY_DEST/COPY_SOURCE as needed.
		if (IsValidD3D12ResourceState(DstOldState))
			TransitionSubResource(D3D12Dst->GetResource(), DstOldState, 0, false);
		if (IsValidD3D12ResourceState(SrcOldState))
			TransitionSubResource(D3D12Src->GetResource(), SrcOldState, 0, false);
		// Flush restore transitions before subsequent passes; GPU-based validation is stricter about leaving
		// COPY_* states visible across unrelated commands than retail scheduling.
		CommandListHandle.FlushResourceBarriers();
	}

	void D3D12CommandContext::RHICopyResource2D(std::shared_ptr< RHITexture2D> DstTex, std::shared_ptr< RHITexture2D> SrcTex, core::vec4u rect)
	{
		auto D3D12Src = RHIResourceCast(SrcTex.get());
		auto D3D12Dst = RHIResourceCast(DstTex.get());
		if (!D3D12Src || !D3D12Dst)
			return;
		const D3D12_RESOURCE_STATES SrcOldState = CommandListHandle.GetResourceState(D3D12Src->GetResource()).GetSubresourceState(0);
		const D3D12_RESOURCE_STATES DstOldState = CommandListHandle.GetResourceState(D3D12Dst->GetResource()).GetSubresourceState(0);
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
		++numCopies;

		if (IsValidD3D12ResourceState(SrcOldState))
			TransitionSubResource(D3D12Src->GetResource(), SrcOldState, 0, false);
		if (IsValidD3D12ResourceState(DstOldState))
			TransitionSubResource(D3D12Dst->GetResource(), DstOldState, 0, false);
		CommandListHandle.FlushResourceBarriers();
	}

	void D3D12CommandContext::FlushCommands(bool WaitForCompletion /*= false*/)
	{
		D3D12RHI_CheckRecordingAllowed("FlushCommands");
		if (!CommandListHandle)
			return;

		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		const ED3D12CommandQueueType QueueType = GetCommandListManager().GetQueueType();
		const bool bHasPendingWork = Device ? Device->HasPendingCommandLists(QueueType) : false;
		// Important: don't rely solely on draw/dispatch counters. We may have "real work" recorded
		// (e.g. pending transition barriers) even when counters are 0. Skipping submit in that case
		// prevents fence progress and makes transient allocations appear to "leak" unless Flush(true) is used.
		const bool bHasPendingBarriers = (CommandListHandle.PendingResourceBarriers().size() > 0);
		const bool bHasDoneWork = HasRecordedCommands() || bHasPendingBarriers || bHasPendingWork;
		const bool bOpenNewCmdList = WaitForCompletion || bHasDoneWork;

		if (!bOpenNewCmdList)
			return;

		CloseCommandList();

		if (Device && bHasPendingWork)
		{
			Device->EnqueuePendingCommandList(std::move(CommandListHandle), QueueType);
			ENQUEUE_RHI_SUBMIT_COMMAND(FlushCommands_ExecutePending,
				Device->ExecutePendingCommandLists(QueueType, WaitForCompletion);
			);
			OpenCommandList();
			return;
		}

		ENQUEUE_RHI_SUBMIT_COMMAND(FlushCommands_ExecuteAndClear,
			CommandListHandle.ExecuteAndClear(WaitForCompletion);
		);
		OpenCommandList();
	}

	void D3D12CommandContext::Finish(std::vector<D3D12CommandListHandle>& OutCommandLists)
	{
		D3D12RHI_CheckRecordingAllowed("Finish");
		if (!CommandListHandle)
			return;
		CloseCommandList();

		if (HasRecordedCommands())
			OutCommandLists.push_back(std::move(CommandListHandle));
		else
			GetCommandListManager().ReleaseCommandList(CommandListHandle);

		CommandListHandle = {};
	}

	void D3D12CommandContext::RHITransitionResource(std::shared_ptr< RHITexture2D> Tex, int32_t NewState, bool Flush /*= false*/)
	{
		auto TexRHI = RHIResourceCast(Tex.get());
		if (TexRHI)
			TransitionResource(TexRHI->GetResource(), (D3D12_RESOURCE_STATES)NewState, Flush);
	}

	void D3D12CommandContext::RDGApplyPassBeginBarriers(const FRDGTextureBarrierDesc* Items, size_t Count, ERDGPassQueue PassQueue)
	{
		if (!Items || Count == 0 || !CommandListHandle)
			return;

		const bool bAsyncCompute = (PassQueue == ERDGPassQueue::AsyncCompute);

		for (size_t i = 0; i < Count; ++i)
		{
			const FRDGTextureBarrierDesc& D = Items[i];
			if (!D.Texture || D.Access == FRDGResourceAccess::Unknown)
				continue;

			D3D12Texture2D* Tex2D = RHIResourceCast(D.Texture.get());
			if (!Tex2D)
				continue;
			FD3D12Resource* Res = Tex2D->GetResource();
			if (!Res || !Res->RequiresResourceStateTracking())
				continue;

			D3D12_RESOURCE_STATES Target = D3D12_RESOURCE_STATE_COMMON;
			bool bSrvDepthPlaneOnlyBarrier = false;
			switch (D.Access)
			{
			case FRDGResourceAccess::SRV:
				Target = ShaderReadableStateForTexture2DSample(Tex2D, bAsyncCompute);
				bSrvDepthPlaneOnlyBarrier = ShouldTransitionDepthPlaneOnlyForShaderSample(Tex2D, Res);
				break;
			case FRDGResourceAccess::UAV:
				Target = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				break;
			case FRDGResourceAccess::RTV:
				Target = D3D12_RESOURCE_STATE_RENDER_TARGET;
				break;
			case FRDGResourceAccess::DSV:
				Target = D3D12_RESOURCE_STATE_DEPTH_WRITE;
				break;
			case FRDGResourceAccess::CopySrc:
				Target = D3D12_RESOURCE_STATE_COPY_SOURCE;
				break;
			case FRDGResourceAccess::CopyDst:
				Target = D3D12_RESOURCE_STATE_COPY_DEST;
				break;
			default:
				continue;
			}

			if (D.SubresourceIndex == 0xFFFFFFFFu)
			{
				if (bSrvDepthPlaneOnlyBarrier)
					TransitionSubResource(Res, Target, 0, false);
				else
					TransitionResource(Res, Target, false);
			}
			else
				TransitionSubResource(Res, Target, D.SubresourceIndex, false);
		}

		CommandListHandle.FlushResourceBarriers();
	}

	void D3D12CommandContext::RDGBeginGpuPassTimingFrame()
	{
		if (!IsDefaultContext() || IsAsyncComputeContext())
			return;
		std::shared_ptr<FD3D12Device> Dev = GetParentDevice();
		FD3D12GpuTimestampRing* Ring = Dev ? Dev->GetGpuPassTimestampsRing() : nullptr;
		if (!Ring)
			return;
		ID3D12GraphicsCommandList* Cmd = GetCurrentCommandListHandle().GraphicsCommandList();
		if (!Cmd)
			return;
		Ring->BeginRecording(Cmd);
	}

	void D3D12CommandContext::RDGWriteGpuTimestampAfterPass(const char* PassNameUtf8)
	{
		if (!IsDefaultContext() || IsAsyncComputeContext())
			return;
		std::shared_ptr<FD3D12Device> Dev = GetParentDevice();
		FD3D12GpuTimestampRing* Ring = Dev ? Dev->GetGpuPassTimestampsRing() : nullptr;
		if (!Ring)
			return;
		ID3D12GraphicsCommandList* Cmd = GetCurrentCommandListHandle().GraphicsCommandList();
		if (!Cmd)
			return;
		Ring->AfterPass(Cmd, PassNameUtf8 ? PassNameUtf8 : "");
	}

	void D3D12CommandContext::RDGResolveGpuPassTimingsEndOfFrame()
	{
		if (!IsDefaultContext() || IsAsyncComputeContext())
			return;
		std::shared_ptr<FD3D12Device> Dev = GetParentDevice();
		FD3D12GpuTimestampRing* Ring = Dev ? Dev->GetGpuPassTimestampsRing() : nullptr;
		if (!Ring)
			return;
		ID3D12GraphicsCommandList* Cmd = GetCurrentCommandListHandle().GraphicsCommandList();
		if (!Cmd)
			return;
		Ring->EndRecordingResolve(Cmd);
	}

	void D3D12CommandContext::RDGTryConsumePreviousFrameGpuPassTimings(std::vector<std::pair<std::string, double>>& OutPassGpuMs)
	{
		if (!IsDefaultContext() || IsAsyncComputeContext())
			return;
		std::shared_ptr<FD3D12Device> Dev = GetParentDevice();
		FD3D12GpuTimestampRing* Ring = Dev ? Dev->GetGpuPassTimestampsRing() : nullptr;
		if (!Ring)
			return;
		Ring->TryConsume(OutPassGpuMs);
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

	void D3D12CommandContext::ReleaseCommandAllocator()
	{
		D3D12RHI_CheckRecordingAllowed("ReleaseCommandAllocator");
		if (CommandAllocator != nullptr)
		{
			CommandAllocatorManager.ReleaseCommandAllocator(CommandAllocator);
			CommandAllocator = nullptr;
		}
	}

	std::shared_ptr<RenderCore::FD3D12Device> D3D12CommandContext::GetParentDevice() const
	{
		return GetParentAdapter()->GetDevice();
	}

	void D3D12CommandContext::EnsureStateCache()
	{
		if (CurrentStateCache)
			return;
		auto Dev = GetParentDevice();
		if (!Dev)
			return;
		CurrentStateCache = std::make_shared<FD3D12StateCache>(Dev, this->shared_from_this());
	}

	void D3D12CommandContext::OpenCommandList()
	{
		D3D12RHI_CheckRecordingAllowed("OpenCommandList");
		// Conditionally get a new command allocator.
		// Each command context uses a new allocator for all command lists within a "frame".
		ConditionalObtainCommandAllocator();

		// Get a new command list
		CommandListHandle = GetCommandListManager().ObtainCommandList(*CommandAllocator);
		CommandListHandle.SetCurrentOwningContext(this);

		// Command list Reset clears bindings; caches detect Reset via command-list reset serial.
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
		D3D12RHI_CheckRecordingAllowed("TransitionResource");
		// Per-command-list state: TBD → pending. If !Cl.AreAllSubresourcesSame(), never use ALL_SUBRESOURCES
		// on pending (per-sub only) so GetResourceBarrierCommandList can resolve PRB.SubResource without expanding ALL.
		if (!CommandListHandle)
			return;
		if (!Resource->RequiresResourceStateTracking())
			return;

		const uint16_t SubresourceCount = Resource->GetSubresourceCount();
		if (SubresourceCount == 0)
			return;

		CResourceState& Cl = CommandListHandle.GetResourceState(Resource);

		bool bDidImmediate = false;
		bool bDidPending = false;

		if (!Cl.AreAllSubresourcesSame())
		{
			for (uint16_t SubresourceIndex = 0; SubresourceIndex < SubresourceCount; ++SubresourceIndex)
			{
				const D3D12_RESOURCE_STATES Before = Cl.GetSubresourceState(SubresourceIndex);
				if (Before == D3D12_RESOURCE_STATE_TBD)
				{
					CommandListHandle.AddPendingResourceBarrier(Resource, NewState, SubresourceIndex);
					Cl.SetSubresourceState(SubresourceIndex, NewState);
					bDidPending = true;
				}
				else if (Before != NewState)
				{
					CommandListHandle.AddTransitionBarrier(Resource, Before, NewState, SubresourceIndex);
					Cl.SetSubresourceState(SubresourceIndex, NewState);
					bDidImmediate = true;
				}
			}

			// Only collapse to uniform per-resource tracking if every subresource really reached NewState.
			// Unconditional SetResourceState here overwrote per-sub states and produced bogus Before in barriers (#523).
			if (Cl.CheckResourceState(NewState))
			{
				Cl.SetResourceState(NewState);
			}
		}
		else
		{
			const D3D12_RESOURCE_STATES Before = Cl.GetSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			if (Before == D3D12_RESOURCE_STATE_TBD)
			{
				CommandListHandle.AddPendingResourceBarrier(Resource, NewState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
				Cl.SetResourceState(NewState);
				bDidPending = true;
			}
			else if (Before != NewState)
			{
				CommandListHandle.AddTransitionBarrier(Resource, Before, NewState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
				Cl.SetResourceState(NewState);
				bDidImmediate = true;
			}
		}

		// Immediate transitions are counted inside AddTransitionBarrier; do not double-count here.
		if (Flush && (bDidImmediate || bDidPending))
			CommandListHandle.FlushResourceBarriers();
	}

	void D3D12CommandContext::TransitionSubResource(FD3D12Resource* Resource, D3D12_RESOURCE_STATES NewState, uint32_t Subresource, bool Flush)
	{
		D3D12RHI_CheckRecordingAllowed("TransitionSubResource");
		Assert(Subresource < Resource->GetSubresourceCount());
		if (!CommandListHandle || !Resource->RequiresResourceStateTracking())
			return;

		CResourceState& Cl = CommandListHandle.GetResourceState(Resource);
		const D3D12_RESOURCE_STATES Before = Cl.GetSubresourceState(Subresource);

		if (Before == D3D12_RESOURCE_STATE_TBD)
		{
			CommandListHandle.AddPendingResourceBarrier(Resource, NewState, Subresource);
			Cl.SetSubresourceState(Subresource, NewState);
			if (Flush)
				CommandListHandle.FlushResourceBarriers();
			return;
		}

		if (Before != NewState)
		{
			CommandListHandle.AddTransitionBarrier(Resource, Before, NewState, Subresource);
			if (Flush)
				CommandListHandle.FlushResourceBarriers();
			Cl.SetSubresourceState(Subresource, NewState);
		}
	}

	void D3D12CommandContext::InitializeTexture(FD3D12Resource* Dest, UINT NumSubResources, D3D12_SUBRESOURCE_DATA SubData[])
	{
		D3D12RHI_ScopedRecordingContext ScopedOutsideFrame(RenderCore::ERHIRecordingContextScope::OutsideFrameResourceUpload);
		Assert(Dest);
		D3D12CommandAllocator* TempCommandAllocator = CommandAllocatorManager.ObtainCommandAllocator();
		// Get a new command list
		auto CommandList = GetCommandListManager().ObtainCommandList(*TempCommandAllocator);
		CommandList.SetCurrentOwningContext(this);

		size_t UploadBufferSize = (size_t)GetRequiredIntermediateSize(Dest->GetResource(), 0, NumSubResources);
#if WITH_D3D12_MEMMON
		Render::D3D12CallStats::AddUploadBytes((uint64_t)UploadBufferSize);
#endif
		// UpdateSubresources requires the intermediate offset to be aligned to D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT.
		FAllocation Allocation = CommandList.GetLinearAllocator(UploadFastAllocator).Allocate(UploadBufferSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
		UpdateSubresources(CommandList.GraphicsCommandList(), Dest->GetResource(), Allocation.D3D12Resource, (UINT64)Allocation.Offset, 0, NumSubResources, SubData);
#if WITH_D3D12_MEMMON
		Render::D3D12CallStats::AddCopyBytes((uint64_t)UploadBufferSize);
#endif
		CommandList.AddTransitionBarrier(Dest, D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		Dest->GetResourceState().SetResourceState(D3D12_RESOURCE_STATE_GENERIC_READ);
		CommandList.Close();
		CommandList.ExecuteAndClear(true);
		CommandAllocatorManager.ReleaseCommandAllocator(TempCommandAllocator);
	}

	void D3D12CommandContext::InitializeBuffer(FD3D12Resource* Dest, const void* Data, uint32_t NumBytes, size_t Offset /*= 0*/)
	{
		D3D12RHI_ScopedRecordingContext ScopedOutsideFrame(RenderCore::ERHIRecordingContextScope::OutsideFrameResourceUpload);
		Assert(Dest);
		D3D12CommandAllocator* TempCommandAllocator = CommandAllocatorManager.ObtainCommandAllocator();
		// Get a new command list
		auto CommandList = GetCommandListManager().ObtainCommandList(*TempCommandAllocator);
		CommandList.SetCurrentOwningContext(this);

#if WITH_D3D12_MEMMON
		Render::D3D12CallStats::AddUploadBytes((uint64_t)NumBytes);
#endif
		FAllocation Allocation = CommandList.GetLinearAllocator(UploadFastAllocator).Allocate(NumBytes);
		memcpy(Allocation.CPU, Data, NumBytes);

		D3D12_RESOURCE_STATES OldState = Dest->GetResourceState().GetSubresourceState(0);
		if (OldState != D3D12_RESOURCE_STATE_COPY_DEST)
		{
			CommandList.AddTransitionBarrier(Dest, OldState, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			CommandList.FlushResourceBarriers();
		}

#if WITH_D3D12_MEMMON
		Render::D3D12CallStats::AddCopyBytes((uint64_t)NumBytes);
#endif
		CommandList->CopyBufferRegion(Dest->GetResource(), Offset, Allocation.D3D12Resource, (UINT64)Allocation.Offset, NumBytes);
		CommandList.AddTransitionBarrier(Dest, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		Dest->GetResourceState().SetResourceState(D3D12_RESOURCE_STATE_GENERIC_READ);
		CommandList.Close();
		CommandList.ExecuteAndClear(true);
		CommandAllocatorManager.ReleaseCommandAllocator(TempCommandAllocator);
	}

	FD3D12LinearAllocator& D3D12CommandContext::GetLinearAllocator(EFastAllocatorType type)
	{
		Assert(type == UploadFastAllocator || type == DefaultFastAllocator);
		Assert(CommandListHandle != nullptr);
		return CommandListHandle.GetLinearAllocator(type);
	}

	void D3D12CommandContext::Initialize(void)
	{
	}

	void D3D12CommandContext::Destroy()
	{
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

	void D3D12CommandContext::RHIClearState()
	{
		ClearState();
	}

	void D3D12CommandContext::CleanupUsedHeaps(uint64_t FenceValue, ED3D12CommandQueueType QueueType)
	{
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
