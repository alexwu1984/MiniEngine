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
#include "RHI/RHI.h"
#include "D3D12/D3D12PresentStats.h"
#include "D3D12/D3D12CallStats.h"
#include "D3D12/D3D12RHICommon.h"
#include <dxgi1_4.h>
#include <windows.h>

namespace RenderCore
{
	// Match Microsoft MiniEngine / UE flip-discard swapchain depth.
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
		PresentEndFence.reset();
		PresentEndFenceLastSignaled = 0;
		BackBuffers.clear();
	}

	void D3D12ViewPort::Init()
	{
		auto Adapter = GetParentAdapter();

		// Align with UE 4.26 WindowsD3D12Viewport: no DXGI waitable object; frame pacing via fence after Present.
		bAllowTearing = false;

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
						  L"[D3D12] SwapChainCreate w=%u h=%u fmt=%u buffers=%u flipDiscard=1 flags=0x%08x vsync=1 tearing=%d (UE-style, no waitable)",
						  (unsigned)Desc1.Width,
						  (unsigned)Desc1.Height,
						  (unsigned)Desc1.Format,
						  (unsigned)Desc1.BufferCount,
						  (unsigned)Desc1.Flags,
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

	void D3D12ViewPort::Present()
	{
		if (!SwapChain4)
			return;

		if (D3D12RHI_ShouldUseImGui())
		{
			ImGui::Render();
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GetDefaultCommandContext()->GetCurrentCommandListHandle().GraphicsCommandList());
		}

		GetDefaultCommandContext()->RHIEndDrawing();
		std::shared_ptr<D3D12Texture2D> BackBufTex2D = BackBuffers[FrameIndex];
		GetDefaultCommandContext()->TransitionResource(BackBufTex2D->GetResource(), D3D12_RESOURCE_STATE_PRESENT, false);

		GetDefaultCommandContext()->FlushCommands(true);
		if (auto AsyncCtx = GetDefaultAsyncComputeContext(); AsyncCtx && AsyncCtx->HasRecordedCommands())
			AsyncCtx->FlushCommands(false);

		const UINT syncInterval = 1u;
		const UINT presentFlags = 0u;

		const bool memMon = RenderCore::D3D12RHI_ShouldEnableMemMon();
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

		// Match UE default (r.FinishCurrentFrame == false): wait previous frame completion, then signal.
		if (PresentEndFence && (SUCCEEDED(hrPresent) || hrPresent == DXGI_STATUS_OCCLUDED))
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
