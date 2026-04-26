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
#include "pix.h"
#include "core/logger.h"
#include <windows.h>
#include <dxgi1_4.h>
#include <heapapi.h>
#include "core/commandline.h"

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
		CommandListHandle.FlushResourceBarriers();
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
			CommandListHandle.FlushResourceBarriers();
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
			CommandListHandle.FlushResourceBarriers();
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
		
	}

	void D3D12CommandContext::RHIEndDrawing()
	{
		// Lightweight runtime diagnostics for leak triage (Release differs from Debug Layer behavior).
		if (!IsDefaultContext())
			return;

		static uint64_t sFrame = 0;
		++sFrame;
		if ((sFrame % 120ull) != 0ull)
			return;

		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		if (!Device)
			return;

		const auto Pools = Device->GetDynamicDescriptorHeapPools();
		const std::size_t DynViewHeaps = Pools.CreatedTracking[0].size();
		const std::size_t DynSamplerHeaps = Pools.CreatedTracking[1].size();

		const auto CpuW = Device->GetLinearPageManager(ELinearAllocatorType::CpuWritable);
		const auto GpuX = Device->GetLinearPageManager(ELinearAllocatorType::GpuExclusive);

		std::size_t RS = 0, GPSO = 0, CPSO = 0;
		if (CurrentStateCache)
		{
			RS = CurrentStateCache->GetRootSignatureCacheSize();
			GPSO = CurrentStateCache->GetGraphicsPSOCacheSize();
			CPSO = CurrentStateCache->GetComputePSOCacheSize();
		}

		std::size_t TexCaches = 0, VS = 0, PS = 0, CS = 0;
		if (auto Adapter = TryGetParentAdapter())
		{
			if (auto Rhi = std::dynamic_pointer_cast<D3D12DynamicRHI>(Adapter->GetOwningRHI()))
			{
				const auto Stats = Rhi->GetCacheStats();
				TexCaches = Stats.TexCaches;
				VS = Stats.VS;
				PS = Stats.PS;
				CS = Stats.CS;
			}
		}

		// CPU descriptor allocator heap counts: CBV/SRV/UAV, Sampler, RTV, DSV.
		// Note: legacy AllocateDescriptor call sites can still cause growth; this log helps spot it.
		core::LOG(core::log_inf,
				  L"[D3D12] Frame=%llu DynHeaps(View=%zu Sampler=%zu) "
				  L"Pages(CpuStd=%zu CpuLarge=%zu CpuRet=%zu GpuStd=%zu GpuLarge=%zu GpuRet=%zu)",
				  (unsigned long long)sFrame,
				  DynViewHeaps, DynSamplerHeaps,
				  CpuW.GetStandardPageCount(), CpuW.GetLargePageCount(), CpuW.GetRetiredPageCount(),
				  GpuX.GetStandardPageCount(), GpuX.GetLargePageCount(), GpuX.GetRetiredPageCount());

		// If any of these climb steadily during runtime, something is accidentally calling DirectXTex
		// capture/screengrab/loader APIs repeatedly (often implies transient readback/upload allocations).
		core::LOG(core::log_inf,
				  L"[D3D12] DXTex Calls Capture=%llu SaveWIC=%llu SaveDDS=%llu LoadWIC=%llu LoadDDS=%llu",
				  (unsigned long long)DXTexStats::CaptureTextureCalls_D3D12().load(),
				  (unsigned long long)DXTexStats::ScreenGrab_SaveWICCalls_D3D12().load(),
				  (unsigned long long)DXTexStats::ScreenGrab_SaveDDSCalls_D3D12().load(),
				  (unsigned long long)DXTexStats::WICTextureLoader_LoadFromFileCalls_D3D12().load(),
				  (unsigned long long)DXTexStats::DDSTextureLoader_LoadFromFileCalls_D3D12().load());

		core::LOG(core::log_inf,
				  L"[D3D12] CpuDescHeaps Pool=%zu CBVSRVUAV(Heaps=%zu Free=%zu) Sampler(Heaps=%zu Free=%zu) RTV(Heaps=%zu Free=%zu) DSV(Heaps=%zu Free=%zu)",
				  Device->GetCpuDescriptorGlobalPoolSize(),
				  Device->GetCpuDescriptorHeapCount(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV), Device->GetCpuDescriptorFreeBlockCount(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
				  Device->GetCpuDescriptorHeapCount(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER), Device->GetCpuDescriptorFreeBlockCount(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
				  Device->GetCpuDescriptorHeapCount(D3D12_DESCRIPTOR_HEAP_TYPE_RTV), Device->GetCpuDescriptorFreeBlockCount(D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
				  Device->GetCpuDescriptorHeapCount(D3D12_DESCRIPTOR_HEAP_TYPE_DSV), Device->GetCpuDescriptorFreeBlockCount(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));

		core::LOG(core::log_inf,
				  L"[D3D12] Cache RS=%zu PSO(G=%zu C=%zu) Shaders(VS=%zu PS=%zu CS=%zu) TexCaches=%zu",
				  RS, GPSO, CPSO, VS, PS, CS, TexCaches);

		{
			const auto Live = FD3D12Resource::GetLiveStats();
			const double MB = 1024.0 * 1024.0;
			core::LOG(core::log_inf,
					  L"[D3D12] LiveRes Default(Cnt=%llu Bytes=%.1fMB) Upload(Cnt=%llu Bytes=%.1fMB) Readback(Cnt=%llu Bytes=%.1fMB)",
					  (unsigned long long)Live.DefaultCount, (double)Live.DefaultBytes / MB,
					  (unsigned long long)Live.UploadCount, (double)Live.UploadBytes / MB,
					  (unsigned long long)Live.ReadbackCount, (double)Live.ReadbackBytes / MB);

			static uint64_t sPrevCreateCnt = 0, sPrevDestroyCnt = 0, sPrevCreateBytes = 0, sPrevDestroyBytes = 0;
			const uint64_t dCreateCnt = Live.TotalCreateCount - sPrevCreateCnt;
			const uint64_t dDestroyCnt = Live.TotalDestroyCount - sPrevDestroyCnt;
			const uint64_t dCreateBytes = Live.TotalCreateBytes - sPrevCreateBytes;
			const uint64_t dDestroyBytes = Live.TotalDestroyBytes - sPrevDestroyBytes;
			sPrevCreateCnt = Live.TotalCreateCount;
			sPrevDestroyCnt = Live.TotalDestroyCount;
			sPrevCreateBytes = Live.TotalCreateBytes;
			sPrevDestroyBytes = Live.TotalDestroyBytes;

			core::LOG(core::log_inf,
					  L"[D3D12] ResChurn +Create=%llu (%.1fMB) +Destroy=%llu (%.1fMB)",
					  (unsigned long long)dCreateCnt, (double)dCreateBytes / MB,
					  (unsigned long long)dDestroyCnt, (double)dDestroyBytes / MB);
		}

		// Direct D3D12 creates that bypass FD3D12Resource (e.g. linear allocator pages).
		{
			const double MB = 1024.0 * 1024.0;
			static uint64_t sPrevCpuPageUploadCnt = 0, sPrevCpuPageUploadBytes = 0;
			static uint64_t sPrevGpuPageDefaultCnt = 0, sPrevGpuPageDefaultBytes = 0;

			const uint64_t curUploadCnt = D3D12CreateStats::LinearPage_CreateCount_Upload().load(std::memory_order_relaxed);
			const uint64_t curUploadBytes = D3D12CreateStats::LinearPage_CreateBytes_Upload().load(std::memory_order_relaxed);
			const uint64_t curDefaultCnt = D3D12CreateStats::LinearPage_CreateCount_Default().load(std::memory_order_relaxed);
			const uint64_t curDefaultBytes = D3D12CreateStats::LinearPage_CreateBytes_Default().load(std::memory_order_relaxed);

			const uint64_t dUploadCnt = curUploadCnt - sPrevCpuPageUploadCnt;
			const uint64_t dUploadBytes = curUploadBytes - sPrevCpuPageUploadBytes;
			const uint64_t dDefaultCnt = curDefaultCnt - sPrevGpuPageDefaultCnt;
			const uint64_t dDefaultBytes = curDefaultBytes - sPrevGpuPageDefaultBytes;

			sPrevCpuPageUploadCnt = curUploadCnt;
			sPrevCpuPageUploadBytes = curUploadBytes;
			sPrevGpuPageDefaultCnt = curDefaultCnt;
			sPrevGpuPageDefaultBytes = curDefaultBytes;

			core::LOG(core::log_inf,
					  L"[D3D12] DirectCreates LinearPages Upload(+%llu %.1fMB) Default(+%llu %.1fMB)",
					  (unsigned long long)dUploadCnt, (double)dUploadBytes / MB,
					  (unsigned long long)dDefaultCnt, (double)dDefaultBytes / MB);
		}

		// Command list / allocator pressure (these are not FD3D12Resource, but can drive CPU Private + NonLocal growth if not reused).
		{
			auto& DirectMgr = Device->GetCommandListManager(ED3D12CommandQueueType::Default);
			auto& CopyMgr = Device->GetCommandListManager(ED3D12CommandQueueType::Copy);
			auto& AsyncMgr = Device->GetCommandListManager(ED3D12CommandQueueType::Async);

			core::LOG(core::log_inf,
					  L"[D3D12] CmdLists Ready(Direct=%u Copy=%u Async=%u)",
					  DirectMgr.GetReadyListCount(), CopyMgr.GetReadyListCount(), AsyncMgr.GetReadyListCount());

			// Track how many times we actually submit to the driver per sample.
			static uint64_t sPrevSubmitDirect = 0, sPrevSubmitCopy = 0, sPrevSubmitCompute = 0;
			const uint64_t curSubmitDirect = D3D12SubmitStats::SubmitCount_Direct().load(std::memory_order_relaxed);
			const uint64_t curSubmitCopy = D3D12SubmitStats::SubmitCount_Copy().load(std::memory_order_relaxed);
			const uint64_t curSubmitCompute = D3D12SubmitStats::SubmitCount_Compute().load(std::memory_order_relaxed);
			const uint64_t dSubmitDirect = curSubmitDirect - sPrevSubmitDirect;
			const uint64_t dSubmitCopy = curSubmitCopy - sPrevSubmitCopy;
			const uint64_t dSubmitCompute = curSubmitCompute - sPrevSubmitCompute;
			sPrevSubmitDirect = curSubmitDirect;
			sPrevSubmitCopy = curSubmitCopy;
			sPrevSubmitCompute = curSubmitCompute;

			core::LOG(core::log_inf,
					  L"[D3D12] Submits +Direct=%llu +Copy=%llu +Async=%llu",
					  (unsigned long long)dSubmitDirect,
					  (unsigned long long)dSubmitCopy,
					  (unsigned long long)dSubmitCompute);

			// Present behavior (occlusion/minimize can cause Present to return immediately and queue to balloon).
			static uint64_t sPrevPresentCalls = 0, sPrevPresentOcc = 0, sPrevPresentFail = 0, sPrevIconic = 0, sPrevNotVis = 0;
			const uint64_t curPresentCalls = D3D12PresentStats::PresentCalls().load(std::memory_order_relaxed);
			const uint64_t curPresentOcc = D3D12PresentStats::PresentOccluded().load(std::memory_order_relaxed);
			const uint64_t curPresentFail = D3D12PresentStats::PresentFailed().load(std::memory_order_relaxed);
			const uint64_t curIconic = D3D12PresentStats::WindowIconic().load(std::memory_order_relaxed);
			const uint64_t curNotVis = D3D12PresentStats::WindowNotVisible().load(std::memory_order_relaxed);
			const uint64_t dPresentCalls = curPresentCalls - sPrevPresentCalls;
			const uint64_t dPresentOcc = curPresentOcc - sPrevPresentOcc;
			const uint64_t dPresentFail = curPresentFail - sPrevPresentFail;
			const uint64_t dIconic = curIconic - sPrevIconic;
			const uint64_t dNotVis = curNotVis - sPrevNotVis;
			sPrevPresentCalls = curPresentCalls;
			sPrevPresentOcc = curPresentOcc;
			sPrevPresentFail = curPresentFail;
			sPrevIconic = curIconic;
			sPrevNotVis = curNotVis;

			core::LOG(core::log_inf,
					  L"[D3D12] Present +Calls=%llu +Occ=%llu +Fail=%llu Win(+Iconic=%llu +NotVis=%llu)",
					  (unsigned long long)dPresentCalls,
					  (unsigned long long)dPresentOcc,
					  (unsigned long long)dPresentFail,
					  (unsigned long long)dIconic,
					  (unsigned long long)dNotVis);

			// Allocator pressure for this context (if this grows every sample, we are leaking/never reusing command allocators).
			core::LOG(core::log_inf,
					  L"[D3D12] CmdAllocs(Context Total=%zu Available=%zu)",
					  CommandAllocatorManager.GetTotalAllocatorCount(),
					  CommandAllocatorManager.GetAvailableAllocatorCount());
		}

		// Track DXGI video memory usage to distinguish "real" GPU growth vs CPU-side leaks/caches.
		// If LOCAL usage climbs every sample while our internal pools stay flat, the leak is likely a missing GPU fence-based release.
		if (auto Adapter = TryGetParentAdapter())
		{
			// Diagnostic: force a full GPU idle to test whether growth is just frames-in-flight / driver queue caching.
			// This is intentionally opt-in because it will severely impact performance.
			if (core::CommandLine::Get().GetName("d3d12forceidle"))
			{
				Adapter->BlockUntilIdle();
			}

			// Optional: request DXGI to trim internal allocations.
			// This can reduce non-local growth caused by runtime/driver caching in some configurations.
			if (!core::CommandLine::Get().GetName("nodxgitrim"))
			{
				win32::com_ptr<IDXGIDevice3> DxgiDevice3;
				if (SUCCEEDED(Adapter->GetD3DDevice()->QueryInterface(IID_PPV_ARGS(DxgiDevice3.get_init_ref()))) && DxgiDevice3)
				{
					DxgiDevice3->Trim();
				}
			}

			// Fence progress (if Completed lags far behind Current/Signaled, the CPU is outrunning GPU and driver memory can balloon).
			{
				auto& FrameFence = Adapter->GetFrameFence();
				core::LOG(core::log_inf,
						  L"[D3D12] Fence Frame(Cur=%llu Sig=%llu CompFast=%llu CompPeek=%llu)",
						  (unsigned long long)FrameFence.GetCurrentFence(),
						  (unsigned long long)FrameFence.GetLastSignaledFence(),
						  (unsigned long long)FrameFence.GetLastCompletedFenceFast(),
						  (unsigned long long)FrameFence.PeekLastCompletedFence());

				auto& DirectMgr = Device->GetCommandListManager(ED3D12CommandQueueType::Default);
				auto& CopyMgr = Device->GetCommandListManager(ED3D12CommandQueueType::Copy);
				auto& AsyncMgr = Device->GetCommandListManager(ED3D12CommandQueueType::Async);
				auto& DirectFence = DirectMgr.GetFence();
				auto& CopyFence = CopyMgr.GetFence();
				auto& AsyncFence = AsyncMgr.GetFence();
				core::LOG(core::log_inf,
						  L"[D3D12] Fence Queue(Direct Cur=%llu Sig=%llu CompFast=%llu CompPeek=%llu) "
						  L"(Copy Cur=%llu Sig=%llu CompFast=%llu CompPeek=%llu) "
						  L"(Async Cur=%llu Sig=%llu CompFast=%llu CompPeek=%llu)",
						  (unsigned long long)DirectFence.GetCurrentFence(),
						  (unsigned long long)DirectFence.GetLastSignaledFence(),
						  (unsigned long long)DirectFence.GetLastCompletedFenceFast(),
						  (unsigned long long)DirectFence.PeekLastCompletedFence(),
						  (unsigned long long)CopyFence.GetCurrentFence(),
						  (unsigned long long)CopyFence.GetLastSignaledFence(),
						  (unsigned long long)CopyFence.GetLastCompletedFenceFast(),
						  (unsigned long long)CopyFence.PeekLastCompletedFence(),
						  (unsigned long long)AsyncFence.GetCurrentFence(),
						  (unsigned long long)AsyncFence.GetLastSignaledFence(),
						  (unsigned long long)AsyncFence.GetLastCompletedFenceFast(),
						  (unsigned long long)AsyncFence.PeekLastCompletedFence());
			}

			// Fence core pool pressure (if TotalCreated grows, fence cores are not being reused).
			{
				auto& FencePool = Adapter->GetFenceCorePool();
				core::LOG(core::log_inf,
						  L"[D3D12] Fences CorePool(TotalCreated=%llu Available=%u)",
						  (unsigned long long)FencePool.GetTotalCreatedCount(),
						  FencePool.GetAvailableCount());
			}

			// Centralized mem monitor (off by default, enable with d3d12_memmon=1).
			RenderCore::D3D12MemoryMonitor::TickOncePerSecond(Adapter, Device);
		}

		// Virtual memory breakdown moved into D3D12MemoryMonitor (d3d12_memmon=1).
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
		CommandListHandle->DrawIndexedInstanced(IndexBuffer->GetIndexCount(),1,0,0,0);
		++numDraws;
	}

	void D3D12CommandContext::Draw(uint32_t VertexCount, uint32_t VertexStartOffset /*= 0*/)
	{
		if (!CurrentStateCache)
			return;
		if (!CurrentStateCache->ApplyGraphicState(CommandListHandle))
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
		CommandListHandle.FlushResourceBarriers();
	}

	void D3D12CommandContext::RHICopyResource2D(std::shared_ptr< RHITexture2D> DstTex, std::shared_ptr< RHITexture2D> SrcTex, core::vec4u rect)
	{
		auto D3D12Src = RHIResourceCast(SrcTex.get());
		auto D3D12Dst = RHIResourceCast(DstTex.get());
		if (!D3D12Src || !D3D12Dst)
			return;
		auto SrcOldState = D3D12Src->GetResource()->GetResourceState().GetSubresourceState(0);
		auto DstOldState = D3D12Src->GetResource()->GetResourceState().GetSubresourceState(0);
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
		CommandListHandle.FlushResourceBarriers();
	}

	void D3D12CommandContext::FlushCommands(bool WaitForCompletion /*= false*/)
	{
		(void)FlushCommandsGetFence(WaitForCompletion);
	}

	uint64_t D3D12CommandContext::FlushCommandsGetFence(bool WaitForCompletion /*= false*/)
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
			const uint64_t SignaledFenceValue = CommandListHandle.ExecuteAndClear(WaitForCompletion);

			// Get a new command list to replace the one we submitted for execution. 
			// Restore the state from the previous command list.
			OpenCommandList();
			return SignaledFenceValue;
		}
		return 0;
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
		// NOTE:
		// We can't safely use a single ALL_SUBRESOURCES transition barrier with the "before" state
		// coming from just subresource 0. If subresources are in different states, that will produce
		// RESOURCE_BARRIER_BEFORE_AFTER_MISMATCH (#527) and also desync state tracking.
		//
		// Instead, emit correct per-subresource transitions based on tracked state.
		bool bAnyTransition = false;
		const uint16_t SubresourceCount = Resource->GetSubresourceCount();
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
		if (bAnyTransition && Flush)
		{
			CommandListHandle.FlushResourceBarriers();
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

		if (CurrentStateCache && StateCacheMap.empty())
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
