#include "D3D12/D3D12ViewPort.h"
#include "D3D12/D3D12RHIRecording.h"
#include "D3D12/D3D12RHI.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12DescriptorCache.h"
#include "D3D12/D3D12Texture2D.h"
#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12CommandList.h"
#include "Imgui/imgui_impl_dx12.h"
#include "Imgui/imgui_impl_win32.h"
#include "core/commandline.h"
#include "core/logger.h"
#include "RHI/RHI.h"
#include "D3D12/D3D12PresentStats.h"
#include "D3D12/D3D12CallStats.h"
#include "D3D12/D3D12RHICommon.h"
#include "RHI/RHITexture2D.h"

namespace RenderCore
{
	// Default flip-discard swap chain depth (triple-buffered).
	static const uint32_t WindowsDefaultNumBackBuffers = 3;
	static bool D3D12RHI_ShouldUseImGui()
	{
		return !core::CommandLine::Get().GetName("noimgui");
	}

	/** Matches D3D11 viewport: uncapped by default. Pass -vsync to force interval=1 (VSync on). */
	static bool D3D12RHI_WantsVsyncPresent()
	{
		return core::CommandLine::Get().GetSwitch("vsync");
	}

	static bool D3D12_QueryAllowTearing(IDXGIFactory2* Factory2)
	{
		if (!Factory2)
			return false;
		win32::com_ptr<IDXGIFactory5> F5;
		if (FAILED(Factory2->QueryInterface(IID_PPV_ARGS(F5.get_init_ref()))) || !F5)
			return false;
		UINT Allow = 0;
		return SUCCEEDED(F5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &Allow, sizeof(Allow))) && Allow != 0;
	}

	D3D12ViewPort::D3D12ViewPort(std::weak_ptr<FD3D12Adapter> InAdpater, HWND InWindowHandle, uint32_t InSizeX, uint32_t InSizeY)
		:FD3D12AdapterChild(InAdpater),
		WindowHandle(InWindowHandle),
		SizeX(InSizeX),
		SizeY(InSizeY),
		bAllowTearing(false),
		NumBackBuffers(0),
		bIsFullscreen(false),
		PixelFormat(PF_R8G8B8A8),
		FrameIndex(0)
	{
		Init();
	}

	D3D12ViewPort::~D3D12ViewPort()
	{
		// Release backbuffers only after the GPU is idle; otherwise debug layer / driver can stall
		// or fault when RTVs are destroyed while work is still in flight.
		D3D12RHI_ScopedExclusiveRegion RHIExclusiveScope;
		if (std::shared_ptr<FD3D12Adapter> Adapter = TryGetParentAdapter())
		{
			if (Adapter->GetDevice())
			{
				WaitForFrameEventCompletion();
				Adapter->BlockUntilIdle();
			}
		}
		PresentEndFence.reset();
		PresentEndFenceLastSignaled = 0;
		BackBuffers.clear();
	}

	void D3D12ViewPort::Init()
	{
		D3D12RHI_ScopedExclusiveRegion RHIExclusiveScope;
		auto Adapter = GetParentAdapter();

		// Align with D3D11: allow DXGI tearing when supported so Present(0) is valid and avoids ~half-refresh FPS cliffs.
		bAllowTearing = D3D12_QueryAllowTearing(Adapter->GetDXGIFactory2());

		CalculateSwapChainDepth(WindowsDefaultNumBackBuffers);

		UINT SwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		if (bAllowTearing)
			SwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

		const DXGI_MODE_DESC BufferDesc = SetupDXGI_MODE_DESC();
		if (SwapChain1 == nullptr)
		{
			{
				DXGI_SWAP_CHAIN_DESC1 Desc1 = {};
				Desc1.Width = BufferDesc.Width;
				Desc1.Height = BufferDesc.Height;
				Desc1.Format = BufferDesc.Format;
				Desc1.Stereo = FALSE;
				Desc1.SampleDesc.Count = 1;
				Desc1.SampleDesc.Quality = 0;
				Desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
				Desc1.BufferCount = NumBackBuffers;
				Desc1.Scaling = DXGI_SCALING_NONE;
				Desc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
				Desc1.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
				Desc1.Flags = SwapChainFlags;

				ID3D12CommandQueue* pCommandQueue = Adapter->GetDevice()->GetD3DCommandQueue();
				win32::com_ptr<IDXGISwapChain1> NewSwapChain1;
				const HRESULT hrCreate = Adapter->GetDXGIFactory2()->CreateSwapChainForHwnd(
					pCommandQueue,
					WindowHandle,
					&Desc1,
					nullptr,
					nullptr,
					NewSwapChain1.get_init_ref());
				VERIFYD3DRESULT(hrCreate);

				SwapChain1 = NewSwapChain1;
				NewSwapChain1->QueryInterface(IID_PPV_ARGS(SwapChain4.get_init_ref()));

				core::LOG(core::log_inf,
						  L"[D3D12] SwapChainCreate w=%u h=%u fmt=%u buffers=%u flipDiscard=1 flags=0x%08x present=%s tearing=%d (no waitable)",
						  (unsigned)Desc1.Width,
						  (unsigned)Desc1.Height,
						  (unsigned)Desc1.Format,
						  (unsigned)Desc1.BufferCount,
						  (unsigned)Desc1.Flags,
						  D3D12RHI_WantsVsyncPresent() ? L"vsync" : L"uncapped",
						  (int)bAllowTearing);
			}

			Adapter->GetDXGIFactory2()->MakeWindowAssociation(WindowHandle, DXGI_MWA_NO_WINDOW_CHANGES);
		}

		::PostMessage(WindowHandle, WM_PAINT, 0, 0);

		for (int i = 0; i < NumBackBuffers; ++i)
		{
			win32::com_ptr<ID3D12Resource> BackBufferrRes;
			VERIFYD3DRESULT(SwapChain4->GetBuffer(i, IID_PPV_ARGS(BackBufferrRes.get_init_ref())));
			std::shared_ptr<D3D12Texture2D> BackBufTex2D = std::make_shared<D3D12Texture2D>(GetParentAdapter());
			BackBufTex2D->CreateFromSwapChain(L"BackBuffer", BackBufferrRes.get());
			BackBuffers.emplace_back(BackBufTex2D);
		}

		FrameIndex = SwapChain4->GetCurrentBackBufferIndex();

		if (SwapChain4 && !PresentEndFence)
		{
			PresentEndFence = std::make_shared<FD3D12Fence>(GetParentAdapter(), L"ViewportPresentEnd");
			PresentEndFence->CreateFence();
			PresentEndFenceLastSignaled = 0;
		}

		if (D3D12RHI_ShouldUseImGui())
		{
			::ImGui_ImplWin32_Init(WindowHandle);
			ImGuiIO& io = ImGui::GetIO();

			io.FontGlobalScale = ::GetDpiForWindow(WindowHandle) / 96.0f;
			io.FontAllowUserScaling = true;
		}
	}

	void D3D12ViewPort::Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen)
	{
		auto Adapter = GetParentAdapter();
		auto Device = Adapter->GetDevice();
		if (!Device)
			return;

		D3D12RHI_ScopedExclusiveRegion RHIExclusiveScope;

		if (SizeX != InSizeX || SizeY != InSizeY)
		{
			SizeX = InSizeX;
			SizeY = InSizeY;
			Adapter->BlockUntilIdle();
			Device->GetDefaultCommandContext()->ClearState();
			Device->GetDefaultAsyncComputeContext()->ClearState();

			BackBuffers.clear();

			CalculateSwapChainDepth(WindowsDefaultNumBackBuffers);

			UINT SwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			if (bAllowTearing)
				SwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

			HRESULT hr = SwapChain4->ResizeBuffers(NumBackBuffers, SizeX, SizeY, GetRenderTargetFormat(PixelFormat), SwapChainFlags);
			if (FAILED(hr))
				return;

			core::LOG(core::log_inf,
					  L"[D3D12] SwapChainResize w=%u h=%u fmt=%u buffers=%u flags=0x%08x",
					  (unsigned)SizeX,
					  (unsigned)SizeY,
					  (unsigned)GetRenderTargetFormat(PixelFormat),
					  (unsigned)NumBackBuffers,
					  (unsigned)SwapChainFlags);

			for (int i = 0; i < NumBackBuffers; ++i)
			{
				win32::com_ptr<ID3D12Resource> BackBufferrRes;
				VERIFYD3DRESULT(SwapChain4->GetBuffer(i, IID_PPV_ARGS(BackBufferrRes.get_init_ref())));
				std::shared_ptr<D3D12Texture2D> BackBufTex2D = std::make_shared<D3D12Texture2D>(GetParentAdapter());
				BackBufTex2D->CreateFromSwapChain(L"BackBuffer", BackBufferrRes.get());
				BackBuffers.emplace_back(BackBufTex2D);
			}
			FrameIndex = SwapChain4->GetCurrentBackBufferIndex();

			// GPU is idle; next Present will re-establish frame-fence timeline.
			PresentEndFenceLastSignaled = 0;
		}
	}

	core::vec2u D3D12ViewPort::GetSize() const
	{
		return core::vec2u( SizeX,SizeY );
	}

	void D3D12ViewPort::Clear(const core::FLinearColor& Color)
	{
		std::shared_ptr<D3D12Texture2D> BackBufTex2D = BackBuffers[FrameIndex];
		GetDefaultCommandContext()->Clear(BackBufTex2D, nullptr, Color);
	}

	void D3D12ViewPort::SetRenderTarget()
	{
		std::shared_ptr<D3D12Texture2D> BackBufTex2D = BackBuffers[FrameIndex];
		GetDefaultCommandContext()->SetRenderTarget(BackBufTex2D, {});
	}

	void D3D12ViewPort::WaitForFrameEventCompletion()
	{
		if (!PresentEndFence || PresentEndFenceLastSignaled == 0)
			return;
		PresentEndFence->WaitForFence(PresentEndFenceLastSignaled);
	}

	void D3D12ViewPort::IssueFrameEvent()
	{
		if (!PresentEndFence)
			return;
		PresentEndFenceLastSignaled = PresentEndFence->Signal(ED3D12CommandQueueType::Default);
	}

	void D3D12ViewPort::RHIImGuiRenderDrawData()
	{
		if (!SwapChain4 || !D3D12RHI_ShouldUseImGui())
			return;
		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(
			ImGui::GetDrawData(),
			GetDefaultCommandContext()->GetCurrentCommandListHandle().GraphicsCommandList(),
			GetDefaultCommandContext().get());
	}

	void D3D12ViewPort::Present()
	{
		if (!SwapChain4)
			return;
		RHIImGuiRenderDrawData();
		RHISubmitAndPresentFrame();
	}

	void D3D12ViewPort::RHISubmitAndPresentFrame()
	{
		if (!SwapChain4)
			return;

		auto DefaultCtx = GetDefaultCommandContext();
		DefaultCtx->RHIEndDrawing();
		std::shared_ptr<D3D12Texture2D> BackBufTex2D = BackBuffers[FrameIndex];
		// D3D12 validation / GPU-based validation: transitioning to PRESENT while the swap-chain image is still bound
		// as an RTV (typical after Tonemapping + ImGui) is invalid and can wedge the GPU; retail drivers often tolerate it.
		DefaultCtx->SetRenderTarget(std::vector<std::shared_ptr<RHITexture2D>>{}, nullptr);
		DefaultCtx->TransitionResource(BackBufTex2D->GetResource(), D3D12_RESOURCE_STATE_PRESENT, false);

		// Flush barriers, release allocator to pool, clear state, then flush pending work.
		// Returning the allocator lets ObtainCommandAllocator reset it when GPU-ready before the next OpenCommandList.
		if (DefaultCtx->GetCurrentCommandListHandle() != nullptr)
			DefaultCtx->GetCurrentCommandListHandle().FlushResourceBarriers();
		DefaultCtx->ReleaseCommandAllocator();
		DefaultCtx->ClearState();

		// Present path flushes the default context; submission still depends on whether
		// the context/pending lists did meaningful work (FlushCommands internal logic).
		DefaultCtx->FlushCommands(false);

		if (auto AsyncCtx = GetDefaultAsyncComputeContext())
		{
			if (AsyncCtx->GetCurrentCommandListHandle() != nullptr)
				AsyncCtx->GetCurrentCommandListHandle().FlushResourceBarriers();
			AsyncCtx->ReleaseCommandAllocator();
			AsyncCtx->ClearState();
			if (AsyncCtx->HasRecordedCommands())
				AsyncCtx->FlushCommands(false);
		}

		// Budget-based flush: when GPU falls behind, dynamic heaps / upload allocator pages can grow
		// because fence-tied recycling can't keep up. If we exceed thresholds, do a flush + light wait.
		{
			const std::shared_ptr<FD3D12Device> Device = GetParentAdapter()->GetDevice();
			if (Device)
			{
				auto& Pools = Device->GetDynamicDescriptorHeapPools();
				std::size_t DynViewHeaps = 0;
				std::size_t DynSamplerHeaps = 0;
				{
					std::lock_guard<std::mutex> Lock(Pools.Mutex);
					DynViewHeaps = Pools.CreatedTracking[0].size();
					DynSamplerHeaps = Pools.CreatedTracking[1].size();
				}

				auto& UploadPool = Device->GetFastAllocator(UploadFastAllocator);
				auto& DefaultPool = Device->GetFastAllocator(DefaultFastAllocator);

				// Conservative defaults; tune as needed.
				constexpr std::size_t kMaxDynViewHeaps = 256;
				constexpr std::size_t kMaxDynSamplerHeaps = 128;
				constexpr std::size_t kMaxUploadStdPages = 96;
				constexpr std::size_t kMaxUploadRetiredPages = 192;
				constexpr std::size_t kMaxDefaultStdPages = 96;
				constexpr std::size_t kMaxDefaultRetiredPages = 192;

				const bool bOverBudget =
					(DynViewHeaps > kMaxDynViewHeaps) ||
					(DynSamplerHeaps > kMaxDynSamplerHeaps) ||
					(UploadPool.GetStandardPageCount() > kMaxUploadStdPages) ||
					(UploadPool.GetRetiredPageCount() > kMaxUploadRetiredPages) ||
					(DefaultPool.GetStandardPageCount() > kMaxDefaultStdPages) ||
					(DefaultPool.GetRetiredPageCount() > kMaxDefaultRetiredPages);

				if (bOverBudget)
				{
					// When over budget, flush and wait so fences can catch up.
					DefaultCtx->FlushCommands(true);
				}
			}
		}

		// Ordering: back-buffer barriers flushed + default/async queues flushed above → signal frame fence → Present.
		// NotifyEndOfFrameFenceValue lets RHIBeginFrame optionally WaitForFence to cap in-flight GPU frames (RHI thread prep).
		{
			std::shared_ptr<FD3D12Adapter> Adapter = GetParentAdapter();
			auto& FrameFence = Adapter->GetFrameFence();
			const uint64_t endFrameFenceValue = FrameFence.FD3D12Fence::Signal(ED3D12CommandQueueType::Default);
			Adapter->NotifyEndOfFrameFenceValue(endFrameFenceValue);
		}

		// D3D11 uses Present(0); we default the same. -vsync uses interval 1 (caps at display refresh / waits on VBlank).
		const UINT syncInterval = D3D12RHI_WantsVsyncPresent() ? 1u : 0u;
		UINT presentFlags = 0u;
		if (syncInterval == 0u && bAllowTearing)
			presentFlags |= DXGI_PRESENT_ALLOW_TEARING;

		const bool memMon = RenderCore::D3D12RHI_ShouldEnableMemMon();
		if (memMon)
		{
			D3D12PresentStats::PresentCalls().fetch_add(1, std::memory_order_relaxed);
			if (::IsIconic(WindowHandle))
				D3D12PresentStats::WindowIconic().fetch_add(1, std::memory_order_relaxed);
			if (!::IsWindowVisible(WindowHandle))
				D3D12PresentStats::WindowNotVisible().fetch_add(1, std::memory_order_relaxed);
		}

		const HRESULT hrPresent = SwapChain4->Present(syncInterval, presentFlags);
		std::shared_ptr<FD3D12Adapter> Ad = TryGetParentAdapter();
		HRESULT hrRemoved = S_OK;
		if (Ad && Ad->GetD3DDevice())
			hrRemoved = Ad->GetD3DDevice()->GetDeviceRemovedReason();

		const bool bFatalDevice = FAILED(hrPresent) || hrRemoved != S_OK;
		if (bFatalDevice)
		{
			if (Ad)
				if (auto R = Ad->GetOwningRHI())
					R->NotifyFatalDeviceLossFromPresent(hrPresent, hrRemoved);
		}
		if (memMon)
		{
			if (hrPresent == DXGI_STATUS_OCCLUDED)
				D3D12PresentStats::PresentOccluded().fetch_add(1, std::memory_order_relaxed);
			else if (FAILED(hrPresent))
				D3D12PresentStats::PresentFailed().fetch_add(1, std::memory_order_relaxed);
			Render::D3D12CallStats::IncPresent();
		}

		FrameIndex = SwapChain4->GetCurrentBackBufferIndex();

		// Wait for previous frame completion, then signal (equivalent to not finishing the frame before Present).
		if (PresentEndFence && !bFatalDevice && (SUCCEEDED(hrPresent) || hrPresent == DXGI_STATUS_OCCLUDED))
		{
			WaitForFrameEventCompletion();
			IssueFrameEvent();
		}
	}

	void D3D12ViewPort::Prepare()
	{
		if (D3D12RHI_ShouldUseImGui())
		{
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
		}
	}

	std::shared_ptr<RHITexture2D> D3D12ViewPort::GetBackBuffer() const
	{
		if (BackBuffers.empty())
			return {};
		return BackBuffers[FrameIndex];
	}

	void D3D12ViewPort::CalculateSwapChainDepth(int32_t DefaultSwapChainDepth)
	{
		NumBackBuffers = DefaultSwapChainDepth;
	}

	DXGI_MODE_DESC D3D12ViewPort::SetupDXGI_MODE_DESC() const
	{
		DXGI_MODE_DESC Ret;

		Ret.Width = SizeX;
		Ret.Height = SizeY;
		Ret.RefreshRate.Numerator = 0;
		Ret.RefreshRate.Denominator = 0;
		Ret.Format = GetRenderTargetFormat(PixelFormat);
		Ret.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		Ret.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		return Ret;
	}

	std::shared_ptr<D3D12CommandContext> D3D12ViewPort::GetDefaultCommandContext()
	{
		Assert(GetParentAdapter()->GetDevice().get());
		return GetParentAdapter()->GetDevice()->GetDefaultCommandContext();
	}

	std::shared_ptr<D3D12CommandContext> D3D12ViewPort::GetDefaultAsyncComputeContext()
	{
		Assert(GetParentAdapter()->GetDevice().get());
		return GetParentAdapter()->GetDevice()->GetDefaultAsyncComputeContext();
	}

}
