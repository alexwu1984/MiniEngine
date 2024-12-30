#include "D3D12/D3D12ViewPort.h"
#include "D3D12/D3D12RHI.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12Texture2D.h"
#include "D3D12/D3D12CommandContext.h"

namespace RenderCore
{
	static const uint32_t WindowsDefaultNumBackBuffers = 3;

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
		BackBuffers.clear();
	}

	void D3D12ViewPort::Init()
	{
		auto Adapter = GetParentAdapter();

		bAllowTearing = false;
		IDXGIFactory* Factory = Adapter->GetDXGIFactory2();
		if (Factory)
		{
			win32::com_ptr<IDXGIFactory5> Factory5;
			Factory->QueryInterface(IID_PPV_ARGS(Factory5.get_init_ref()));
			if (Factory5.is_valid())
			{
				BOOL AllowTearing;
				if (SUCCEEDED(Factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &AllowTearing, sizeof(AllowTearing))) && AllowTearing)
				{
					bAllowTearing = true;
				}
			}
		}

		CalculateSwapChainDepth(WindowsDefaultNumBackBuffers);

		UINT SwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		if (bAllowTearing)
		{
			SwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		}
		const DXGI_MODE_DESC BufferDesc = SetupDXGI_MODE_DESC();
		// if stereo was not activated or not enabled in settings
		if (SwapChain1 == nullptr)
		{
			// Create the swapchain.
			{
				DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
				SwapChainDesc.BufferDesc = BufferDesc;
				// MSAA Sample count
				SwapChainDesc.SampleDesc.Count = 1;
				SwapChainDesc.SampleDesc.Quality = 0;
				SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
				// 1:single buffering, 2:double buffering, 3:triple buffering
				SwapChainDesc.BufferCount = NumBackBuffers;
				SwapChainDesc.OutputWindow = WindowHandle;
				SwapChainDesc.Windowed = !bIsFullscreen;
				// DXGI_SWAP_EFFECT_DISCARD / DXGI_SWAP_EFFECT_SEQUENTIAL
				SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
				SwapChainDesc.Flags = SwapChainFlags;

				// The command queue used here is irrelevant in regard to multi-GPU as it gets overriden in the Resize
				ID3D12CommandQueue* pCommandQueue = Adapter->GetDevice()->GetD3DCommandQueue();

				win32::com_ptr<IDXGISwapChain> SwapChain;
				HRESULT hrCreate = Adapter->GetDXGIFactory2()->CreateSwapChain(pCommandQueue, &SwapChainDesc, SwapChain.get_init_ref());
				VERIFYD3DRESULT(hrCreate);
				VERIFYD3DRESULT(SwapChain->QueryInterface(IID_PPV_ARGS(SwapChain1.get_init_ref())));

				// Get a SwapChain4 if supported.
				SwapChain->QueryInterface(IID_PPV_ARGS(SwapChain4.get_init_ref()));
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

			uint32_t SwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			if (bAllowTearing)
				SwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

			HRESULT hr = SwapChain4->ResizeBuffers(NumBackBuffers, SizeX, SizeY, GetRenderTargetFormat(PixelFormat), SwapChainFlags);
			if (FAILED(hr))
				return;

			for (int i = 0; i < NumBackBuffers; ++i)
			{
				win32::com_ptr<ID3D12Resource> BackBufferrRes;
				VERIFYD3DRESULT(SwapChain4->GetBuffer(i, IID_PPV_ARGS(BackBufferrRes.get_init_ref())));
				std::shared_ptr<D3D12Texture2D> BackBufTex2D = std::make_shared<D3D12Texture2D>(GetParentAdapter());
				BackBufTex2D->CreateFromSwapChain(L"BackBuffer", BackBufferrRes.get());
				BackBuffers.emplace_back(BackBufTex2D);
			}
			FrameIndex = SwapChain4->GetCurrentBackBufferIndex();
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
		GetDefaultCommandContext()->RHIEndDrawing();
		std::shared_ptr<D3D12Texture2D> BackBufTex2D = BackBuffers[FrameIndex];
		GetDefaultCommandContext()->TransitionResource(BackBufTex2D->GetResource(), D3D12_RESOURCE_STATE_PRESENT, false);
		GetDefaultCommandContext()->FlushCommands(true);
		SwapChain4->Present(1, 0);

		FrameIndex = SwapChain4->GetCurrentBackBufferIndex();
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

	std::shared_ptr<RenderCore::D3D12CommandContext> D3D12ViewPort::GetDefaultCommandContext()
	{
		Assert(GetParentAdapter()->GetDevice().get());
		return GetParentAdapter()->GetDevice()->GetDefaultCommandContext();
	}

}