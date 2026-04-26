#include "D3D12/D3D12ViewPort.h"
#include "D3D12/D3D12RHI.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12Texture2D.h"
#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12CommandList.h"
#include "Imgui/imgui_impl_dx12.h"
#include "Imgui/imgui_impl_win32.h"
#include "core/commandline.h"
#include "core/logger.h"
#include "D3D12/D3D12PresentStats.h"
#include "D3D12/D3D12CallStats.h"
#include <dxgi1_4.h>
#include <windows.h>

namespace RenderCore
{
	static double D3D12RHI_QpcToMicroseconds(LONGLONG dt)
	{
		static LARGE_INTEGER sFreq = {};
		if (sFreq.QuadPart == 0)
			::QueryPerformanceFrequency(&sFreq);
		return (double)dt * 1000000.0 / (double)sFreq.QuadPart;
	}

	static void D3D12RHI_LogFramePacingOncePerSecond(uint64_t WaitableWaits, uint64_t WaitableTimeouts, uint64_t FenceWaits, double FenceWaitMs)
	{
		static ULONGLONG sLastTick = 0;
		static uint64_t sPrevWaitableWaits = 0;
		static uint64_t sPrevWaitableTimeouts = 0;
		static uint64_t sPrevFenceWaits = 0;
		static double sPrevFenceWaitMs = 0.0;

		const ULONGLONG now = ::GetTickCount64();
		if (sLastTick == 0)
			sLastTick = now;
		if (now - sLastTick < 1000)
			return;
		sLastTick = now;

		const uint64_t dWaitableWaits = WaitableWaits - sPrevWaitableWaits;
		const uint64_t dWaitableTimeouts = WaitableTimeouts - sPrevWaitableTimeouts;
		const uint64_t dFenceWaits = FenceWaits - sPrevFenceWaits;
		const double dFenceWaitMs = FenceWaitMs - sPrevFenceWaitMs;

		sPrevWaitableWaits = WaitableWaits;
		sPrevWaitableTimeouts = WaitableTimeouts;
		sPrevFenceWaits = FenceWaits;
		sPrevFenceWaitMs = FenceWaitMs;

		core::LOG(core::log_inf,
				  L"[D3D12] FramePacing Waitable(waits=%llu timeouts=%llu) Fence(waits=%llu waitMs=%.2f)",
				  (unsigned long long)dWaitableWaits,
				  (unsigned long long)dWaitableTimeouts,
				  (unsigned long long)dFenceWaits,
				  dFenceWaitMs);
	}

	// Match Microsoft MiniEngine (DirectX-Graphics-Samples) behavior: triple-buffered flip-discard swapchain.
	static const uint32_t WindowsDefaultNumBackBuffers = 3;
	static bool D3D12RHI_ShouldUseImGui()
	{
		return !core::CommandLine::Get().GetName("noimgui");
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
		if (FrameLatencyWaitableObject)
		{
			::CloseHandle(FrameLatencyWaitableObject);
			FrameLatencyWaitableObject = nullptr;
		}
		BackBuffers.clear();
	}

	void D3D12ViewPort::Init()
	{
		auto Adapter = GetParentAdapter();

		// Sync with Microsoft MiniEngine behavior:
		// - Use a waitable swapchain and cap frame latency to keep CPU from outrunning GPU.
		// - Present with VSync ON (Present(1,0)), no tearing.
		bAllowTearing = false;

		CalculateSwapChainDepth(WindowsDefaultNumBackBuffers);

		// Microsoft MiniEngine uses no ALLOW_MODE_SWITCH flag here.
		const UINT SwapChainFlags =
			(bAllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u) |
			DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

		// Default behavior: rely on VSync (Present(1,0)) for pacing.
		const DXGI_MODE_DESC BufferDesc = SetupDXGI_MODE_DESC();
		// if stereo was not activated or not enabled in settings
		if (SwapChain1 == nullptr)
		{
			// Create the swapchain.
			{
				// Prefer CreateSwapChainForHwnd + DXGI_SWAP_CHAIN_DESC1 for flip-model swapchains.
				// It's more explicit/robust for tearing + frame-latency waitable object behavior.
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
				// Get a SwapChain4 if supported.
				NewSwapChain1->QueryInterface(IID_PPV_ARGS(SwapChain4.get_init_ref()));

				// One-shot evidence: print swapchain parameters for diffing against MiniEngine.
				core::LOG(core::log_inf,
						  L"[D3D12] SwapChainCreate w=%u h=%u fmt=%u buffers=%u flipDiscard=1 flags=0x%08x waitable=1 vsync=1 tearing=%d",
						  (unsigned)Desc1.Width,
						  (unsigned)Desc1.Height,
						  (unsigned)Desc1.Format,
						  (unsigned)Desc1.BufferCount,
						  (unsigned)Desc1.Flags,
						  (int)bAllowTearing);
			}

			// Set the DXGI message hook to not change the window behind our back.
			Adapter->GetDXGIFactory2()->MakeWindowAssociation(WindowHandle, DXGI_MWA_NO_WINDOW_CHANGES);
		}

		// Tell the window to redraw when they can.
		// @todo: For Slate viewports, it doesn't make sense to post WM_PAINT messages (we swallow those.)
		::PostMessage(WindowHandle, WM_PAINT, 0, 0);

		for (int i = 0; i < NumBackBuffers; ++i)
		{
			win32::com_ptr<ID3D12Resource> BackBufferrRes;
			VERIFYD3DRESULT(SwapChain4->GetBuffer(i, IID_PPV_ARGS(BackBufferrRes.get_init_ref())));
			std::shared_ptr<D3D12Texture2D> BackBufTex2D = std::make_shared<D3D12Texture2D>(GetParentAdapter());
			BackBufTex2D->CreateFromSwapChain(L"BackBuffer", BackBufferrRes.get());
			BackBuffers.emplace_back(BackBufTex2D);
		}

		// Initialize to the swapchain's first renderable backbuffer.
		FrameIndex = SwapChain4->GetCurrentBackBufferIndex();

		// MiniEngine-style: cap frame latency and use the waitable object.
		if (SwapChain4)
		{
			const int maxLatency = 1;
			SwapChain4->SetMaximumFrameLatency((UINT)maxLatency);
			core::LOG(core::log_inf, L"[D3D12] SwapChain MaxFrameLatency=%d", maxLatency);

			win32::com_ptr<IDXGISwapChain2> SwapChain2;
			if (SUCCEEDED(SwapChain4->QueryInterface(IID_PPV_ARGS(SwapChain2.get_init_ref()))))
			{
				FrameLatencyWaitableObject = SwapChain2->GetFrameLatencyWaitableObject();
				core::LOG(core::log_inf, L"[D3D12] SwapChain FrameLatencyWaitableObject=%p", FrameLatencyWaitableObject);
			}
		}

		if (D3D12RHI_ShouldUseImGui())
		{
			::ImGui_ImplWin32_Init(WindowHandle);
			ImGuiIO& io = ImGui::GetIO();

			io.FontGlobalScale = ::GetDpiForWindow(WindowHandle) / 96.0f;
			// Allow user UI scaling using CTRL+Mouse Wheel scrolling
			io.FontAllowUserScaling = true;
		}
	}

	void D3D12ViewPort::Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen)
	{
		auto Adapter = GetParentAdapter();
		auto Device = Adapter->GetDevice();
		if (!Device)
			return;

		if (SizeX != InSizeX || SizeY != InSizeY)
		{
			SizeX = InSizeX;
			SizeY = InSizeY;
			Adapter->BlockUntilIdle();
			Device->GetDefaultCommandContext()->ClearState();
			Device->GetDefaultAsyncComputeContext()->ClearState();

			BackBuffers.clear();

			CalculateSwapChainDepth(WindowsDefaultNumBackBuffers);

			// IMPORTANT: keep ResizeBuffers flags consistent with creation.
			const UINT SwapChainFlags =
				(bAllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u) |
				DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

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

			// Re-apply frame latency cap and waitable object after resize.
			if (FrameLatencyWaitableObject)
			{
				::CloseHandle(FrameLatencyWaitableObject);
				FrameLatencyWaitableObject = nullptr;
			}
			SwapChain4->SetMaximumFrameLatency(1);
			win32::com_ptr<IDXGISwapChain2> SwapChain2;
			if (SUCCEEDED(SwapChain4->QueryInterface(IID_PPV_ARGS(SwapChain2.get_init_ref()))))
			{
				FrameLatencyWaitableObject = SwapChain2->GetFrameLatencyWaitableObject();
				core::LOG(core::log_inf, L"[D3D12] SwapChain FrameLatencyWaitableObject=%p", FrameLatencyWaitableObject);
			}
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

	void D3D12ViewPort::Present()
	{
		if (!SwapChain4)
			return;

		// If we created a waitable swapchain, we must wait BEFORE submitting the next frame's work.
		// Waiting after Flush/Execute is too late to apply back-pressure and won't cap driver buffering.
		if (FrameLatencyWaitableObject)
		{
			// Never wait forever here: during shutdown, the render thread can be asked to exit
			// while the swapchain/device is in teardown, and an infinite wait would deadlock exit.
			const DWORD kWaitMs = 16; // ~one frame; enough to apply back-pressure without risking hangs
			const DWORD wr = ::WaitForSingleObjectEx(FrameLatencyWaitableObject, kWaitMs, FALSE);
			if (wr == WAIT_TIMEOUT)
			{
				// If the window is going away or not visible, don't stall shutdown.
				if (!::IsWindow(WindowHandle) || ::IsIconic(WindowHandle) || !::IsWindowVisible(WindowHandle))
					return;
			}
			else if (wr != WAIT_OBJECT_0)
			{
				core::LOG(core::log_inf, L"[D3D12] FrameLatencyWaitable wait result=%u", (unsigned)wr);
			}
		}

		if (D3D12RHI_ShouldUseImGui())
		{
			ImGui::Render();
			//GetDefaultCommandContext()->GetCurrentCommandListHandle()->SetDescriptorHeaps(1, HeapsToBind);
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GetDefaultCommandContext()->GetCurrentCommandListHandle().GraphicsCommandList());
		}

		GetDefaultCommandContext()->RHIEndDrawing();
		std::shared_ptr<D3D12Texture2D> BackBufTex2D = BackBuffers[FrameIndex];
		GetDefaultCommandContext()->TransitionResource(BackBufTex2D->GetResource(), D3D12_RESOURCE_STATE_PRESENT, false);
		
		// Flush commands without waiting for completion to maximize performance.
		GetDefaultCommandContext()->FlushCommands(false);
		// Only flush async compute when it actually recorded work.
		// Submitting empty compute command lists every frame is unnecessary and can amplify driver/runtime internal allocations.
		if (auto AsyncCtx = GetDefaultAsyncComputeContext(); AsyncCtx && AsyncCtx->HasRecordedCommands())
			AsyncCtx->FlushCommands(false);

		// MiniEngine-style: for the Direct queue, signal the deferred frame fence once per frame.
		// This collapses many per-submit Signal() calls into one, reducing driver overhead.
		{
			auto Device = GetParentAdapter()->GetDevice();
			auto& DirectMgr = Device->GetCommandListManager(ED3D12CommandQueueType::Default);
			DirectMgr.SignalDeferredFrameFenceIfNeeded();
		}
		
		// MiniEngine-style present: VSync ON, no tearing.
		const UINT syncInterval = 1u;
		const UINT presentFlags = 0u;

		const bool memMon = core::CommandLine::Get().GetName("d3d12_memmon");
		if (memMon)
		{
			D3D12PresentStats::PresentCalls().fetch_add(1, std::memory_order_relaxed);
			if (::IsIconic(WindowHandle))
				D3D12PresentStats::WindowIconic().fetch_add(1, std::memory_order_relaxed);
			if (!::IsWindowVisible(WindowHandle))
				D3D12PresentStats::WindowNotVisible().fetch_add(1, std::memory_order_relaxed);
		}

		Render::D3D12CallStats::IncPresent();
		const HRESULT hrPresent = SwapChain4->Present(syncInterval, presentFlags);
		if (memMon)
		{
			if (hrPresent == DXGI_STATUS_OCCLUDED)
				D3D12PresentStats::PresentOccluded().fetch_add(1, std::memory_order_relaxed);
			else if (FAILED(hrPresent))
				D3D12PresentStats::PresentFailed().fetch_add(1, std::memory_order_relaxed);
		}

		FrameIndex = SwapChain4->GetCurrentBackBufferIndex();
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
		Ret.RefreshRate.Numerator = 0;	// illamas: use 0 to avoid a potential mismatch with hw
		Ret.RefreshRate.Denominator = 0;	// illamas: ditto
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