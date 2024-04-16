#include "D3D11/D3D11ViewPort.h"
#include "win/com_ptr.h"
#include "D3D11/D3D11RHI.h"
#include "core/logger.h"
#include "D3D11/D3D11Texture2D.h"
#include "Imgui/imgui_impl_win32.h"
#include "Imgui/imgui_impl_dx11.h"

namespace RenderCore
{
	static DXGI_SWAP_EFFECT GSwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	static DXGI_SCALING GSwapScaling = DXGI_SCALING_STRETCH;
	static uint32_t GSwapChainBufferCount = 1;
	static int32_t GD3D11UseAllowTearing = 1;

	uint32_t D3D11ViewPort::GSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;


	static DXGI_FORMAT GetRenderTargetFormat(EPixelFormat PixelFormat)
	{
		DXGI_FORMAT	DXFormat = (DXGI_FORMAT)GPixelFormats[PixelFormat].PlatformFormat;
		switch (DXFormat)
		{
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:		return DXGI_FORMAT_B8G8R8A8_UNORM;
		case DXGI_FORMAT_BC1_TYPELESS:			return DXGI_FORMAT_BC1_UNORM;
		case DXGI_FORMAT_BC2_TYPELESS:			return DXGI_FORMAT_BC2_UNORM;
		case DXGI_FORMAT_BC3_TYPELESS:			return DXGI_FORMAT_BC3_UNORM;
		case DXGI_FORMAT_R16_TYPELESS:			return DXGI_FORMAT_R16_UNORM;
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:		return DXGI_FORMAT_R8G8B8A8_UNORM;
		default: 								return DXFormat;
		}
	}

	struct D3D11ViewPortPrivate
	{
		D3D11DynamicRHI* D3D11RHI;
		uint32_t SizeX;
		uint32_t SizeY;
		uint32_t BackBufferCount;
		HWND WindowHandle = nullptr;
		EPixelFormat PixelFormat = PF_B8G8R8A8;
		win32::com_ptr<IDXGISwapChain> SwapChain;
		win32::com_ptr<ID3D11RenderTargetView> BackBufferRenderTargetView;
		win32::com_ptr<ID3D11Texture2D> BackBufferResource;
		std::shared_ptr<D3D11Texture2D> DepthSRV;
		bool bAllowTearing = false;
	};
	D3D11ViewPort::D3D11ViewPort(D3D11DynamicRHI* D3D11RHI, HWND InWindowHandle, uint32_t InSizeX, uint32_t InSizeY)
		:d_ptr(new D3D11ViewPortPrivate())
	{ 
		C_P(D3D11ViewPort);
		d->D3D11RHI = D3D11RHI;
		d->WindowHandle = InWindowHandle;
		d->SizeX = InSizeX;
		d->SizeY = InSizeY;

		win32::com_ptr<IDXGIDevice> DXGIDevice;
		d->D3D11RHI->GetDevice()->QueryInterface(IID_IDXGIDevice, (void**)DXGIDevice.get_init_ref());

		{
			GSwapEffect = DXGI_SWAP_EFFECT_DISCARD;
			GSwapScaling = DXGI_SCALING_STRETCH;
			GSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			IDXGIFactory1* Factory1 = d->D3D11RHI->GetFactory();
			win32::com_ptr<IDXGIFactory5> Factory5;

			if (GD3D11UseAllowTearing)
			{
				//https://devblogs.microsoft.com/directx/dxgi-flip-model/#:~:text=DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING%20can%20enable%20even%20lower%20latency%20than%20the,using%20the%20DXGI_SCALING%20property%20set%20during%20swapchain%20creation.
				if (S_OK == Factory1->QueryInterface(IID_PPV_ARGS(Factory5.get_init_ref())))
				{
					UINT AllowTearing = 0;
					if (S_OK == Factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &AllowTearing, sizeof(UINT)) && AllowTearing != 0)
					{
						GSwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
						GSwapScaling = DXGI_SCALING_NONE;
						GSwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
						d->bAllowTearing = true;
						GSwapChainBufferCount = 2;
					}
				}
			}
		}


		{
			d->BackBufferCount = GSwapChainBufferCount;

			// Create the swapchain.
			DXGI_SWAP_CHAIN_DESC SwapChainDesc;
			ZeroMemory(&SwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));

			SwapChainDesc.BufferDesc = SetupDXGI_MODE_DESC();
			// MSAA Sample count
			SwapChainDesc.SampleDesc.Count = 1;
			SwapChainDesc.SampleDesc.Quality = 0;
			SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
			// 1:single buffering, 2:double buffering, 3:triple buffering
			SwapChainDesc.BufferCount = d->BackBufferCount;
			SwapChainDesc.OutputWindow = InWindowHandle;
			SwapChainDesc.Windowed = TRUE;
			// DXGI_SWAP_EFFECT_DISCARD / DXGI_SWAP_EFFECT_SEQUENTIAL
			SwapChainDesc.SwapEffect = GSwapEffect;
			SwapChainDesc.Flags = GSwapChainFlags;

			HRESULT CreateSwapChainResult = D3D11RHI->GetFactory()->CreateSwapChain(D3D11RHI->GetDevice(), &SwapChainDesc, d->SwapChain.get_init_ref());
			if (CreateSwapChainResult == E_INVALIDARG)
			{

				core::LOG(core::log_e::log_inf,
					TEXT("CreateSwapChain invalid arguments: \n")
					TEXT(" Size:%ix%i Format:0x%08X \n")
					TEXT(" Windowed:%i SwapEffect:%i Flags: 0x%08X"),
					SwapChainDesc.BufferDesc.Width, SwapChainDesc.BufferDesc.Height, SwapChainDesc.BufferDesc.Format,
					SwapChainDesc.Windowed, SwapChainDesc.SwapEffect, SwapChainDesc.Flags);
			}
			VERIFYD3D11RESULT(CreateSwapChainResult);
			GetSwapChainSurface();
		}

		d->DepthSRV = std::make_shared<D3D11Texture2D>(d->D3D11RHI);
		d->DepthSRV->CreateD3D11Texture2D(RenderCore::PF_DepthStencil, ETextureCreateFlags::TexCreate_DepthStencilTargetable , InSizeX, InSizeY);

		::ImGui_ImplWin32_Init(InWindowHandle);
	}

	D3D11ViewPort::~D3D11ViewPort()
	{
		delete d_ptr;
	}

	void* D3D11ViewPort::GetNativeSwapChain() const
	{
		C_P(const D3D11ViewPort);
		return d->SwapChain.get();
	}

	void* D3D11ViewPort::GetNativeBackBufferTexture() const
	{
		C_P(const D3D11ViewPort);
		return d->BackBufferResource.get();
	}

	void* D3D11ViewPort::GetNativeBackBufferRT() const
	{
		C_P(const D3D11ViewPort);
		return d->BackBufferRenderTargetView.get();
	}

	void D3D11ViewPort::SetRenderTarget()
	{
		C_P(D3D11ViewPort);
		d->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(1, &d->BackBufferRenderTargetView, d->DepthSRV->GetDSV());
	}

	void D3D11ViewPort::Prepare()
	{
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void D3D11ViewPort::Clear(const core::FLinearColor& Color)
	{
		C_P(D3D11ViewPort);
		d->D3D11RHI->GetDeviceContext()->ClearRenderTargetView(d->BackBufferRenderTargetView.get(), &Color.R);
		d->D3D11RHI->GetDeviceContext()->ClearDepthStencilView(d->DepthSRV->GetDSV(), D3D11_CLEAR_DEPTH, 1.0f, 0.f);
	}

	void D3D11ViewPort::Present()
	{
		ImGui::Render();
		::ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		C_P(D3D11ViewPort);
		d->D3D11RHI->GetDefaultCommandContext()->RHIEndDrawing();
		if (d->SwapChain)
		{
			// Present the back buffer to the viewport window.
			uint32_t Flags = 0;
			if ((GetSwapChainFlags() & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) != 0 )
			{
				Flags |= DXGI_PRESENT_ALLOW_TEARING;
			}
			auto Result = d->SwapChain->Present(0, Flags);

			//Impl->SwapChain->Present(0, 0);
		}
	}

	void D3D11ViewPort::Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen)
	{
		C_P(D3D11ViewPort);
		d->D3D11RHI->ClearState();
		d->D3D11RHI->GetDeviceContext()->Flush();
		d->BackBufferResource = {};
		d->BackBufferRenderTargetView = {};

		if (d->SizeX != InSizeX || d->SizeY != InSizeY )
		{
			d->SizeX = InSizeX;
			d->SizeY = InSizeY;

			// Resize the swap chain.

			const UINT SwapChainFlags = GetSwapChainFlags();
			const DXGI_FORMAT RenderTargetFormat = GetRenderTargetFormat(d->PixelFormat);
			
			// Resize all existing buffers, don't change count
			VERIFYD3D11RESULT(d->SwapChain->ResizeBuffers(0, d->SizeX, d->SizeY, RenderTargetFormat, SwapChainFlags));

			if (bInIsFullscreen)
			{
				DXGI_MODE_DESC BufferDesc = SetupDXGI_MODE_DESC();

				if (FAILED(d->SwapChain->ResizeTarget(&BufferDesc)))
				{
					//ResetSwapChainInternal(true);
					VERIFYD3D11RESULT(d->SwapChain->ResizeBuffers(0, d->SizeX, d->SizeY, RenderTargetFormat, SwapChainFlags));

				}
			}
			GetSwapChainSurface();
			d->DepthSRV = std::make_shared<D3D11Texture2D>(d->D3D11RHI);
			d->DepthSRV->CreateD3D11Texture2D(RenderCore::PF_DepthStencil, ETextureCreateFlags::TexCreate_DepthStencilTargetable, InSizeX, InSizeY);
		}
	}

	core::vec2u D3D11ViewPort::GetSize() const
	{
		C_P(const D3D11ViewPort);
		return core::vec2u(d->SizeX,d->SizeY);
	}

	DXGI_MODE_DESC D3D11ViewPort::SetupDXGI_MODE_DESC() const
	{
		C_P(const D3D11ViewPort);
		DXGI_MODE_DESC Ret;
		ZeroMemory(&Ret, sizeof(Ret));
		Ret.Width = d->SizeX;
		Ret.Height = d->SizeY;
		Ret.RefreshRate.Numerator = 60;	
		Ret.RefreshRate.Denominator = 1;	
		Ret.Format = GetRenderTargetFormat(d->PixelFormat);
		//Ret.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		//Ret.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		return Ret;
	}

	void D3D11ViewPort::GetSwapChainSurface()
	{
		C_P(D3D11ViewPort);
		VERIFYD3D11RESULT(d->SwapChain->GetBuffer(0, IID_ID3D11Texture2D, (void**)d->BackBufferResource.get_init_ref()));

		D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
		RTVDesc.Format = DXGI_FORMAT_UNKNOWN;
		RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		RTVDesc.Texture2D.MipSlice = 0;
		VERIFYD3D11RESULT(d->D3D11RHI->GetDevice()->CreateRenderTargetView(d->BackBufferResource.get(), &RTVDesc, d->BackBufferRenderTargetView.get_init_ref()));
	}

	uint32_t D3D11ViewPort::GetSwapChainFlags() const
	{
		C_P(D3D11ViewPort);
		uint32_t SwapChainFlags = GSwapChainFlags;

		// Ensure AllowTearing consistency or ResizeBuffers will fail with E_INVALIDARG
		if ( d->bAllowTearing != !!(SwapChainFlags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING))
		{
			SwapChainFlags ^= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		}

		return SwapChainFlags;
	}


}