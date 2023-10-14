#include "D3D11/D3D11ViewPort.h"
#include "win/com_ptr.h"
#include "D3D11/D3D11RHI.h"
#include "core/logger.h"
#include "D3D11/D3D11Texture2D.h"
#include "Imgui/imgui_impl_win32.h"

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

	struct D3D11ViewPortP
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
		:Impl(new D3D11ViewPortP)
	{ 
		Impl->D3D11RHI = D3D11RHI;
		Impl->WindowHandle = InWindowHandle;
		Impl->SizeX = InSizeX;
		Impl->SizeY = InSizeY;

		win32::com_ptr<IDXGIDevice> DXGIDevice;
		Impl->D3D11RHI->GetDevice()->QueryInterface(IID_IDXGIDevice, (void**)DXGIDevice.get_init_ref());

		{
			GSwapEffect = DXGI_SWAP_EFFECT_DISCARD;
			GSwapScaling = DXGI_SCALING_STRETCH;
			GSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			IDXGIFactory1* Factory1 = Impl->D3D11RHI->GetFactory();
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
						Impl->bAllowTearing = true;
						GSwapChainBufferCount = 2;
					}
				}
			}
		}


		{
			Impl->BackBufferCount = GSwapChainBufferCount;

			// Create the swapchain.
			DXGI_SWAP_CHAIN_DESC SwapChainDesc;
			ZeroMemory(&SwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));

			SwapChainDesc.BufferDesc = SetupDXGI_MODE_DESC();
			// MSAA Sample count
			SwapChainDesc.SampleDesc.Count = 1;
			SwapChainDesc.SampleDesc.Quality = 0;
			SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
			// 1:single buffering, 2:double buffering, 3:triple buffering
			SwapChainDesc.BufferCount = Impl->BackBufferCount;
			SwapChainDesc.OutputWindow = InWindowHandle;
			SwapChainDesc.Windowed = TRUE;
			// DXGI_SWAP_EFFECT_DISCARD / DXGI_SWAP_EFFECT_SEQUENTIAL
			SwapChainDesc.SwapEffect = GSwapEffect;
			SwapChainDesc.Flags = GSwapChainFlags;

			HRESULT CreateSwapChainResult = D3D11RHI->GetFactory()->CreateSwapChain(D3D11RHI->GetDevice(), &SwapChainDesc, Impl->SwapChain.get_init_ref());
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

		Impl->DepthSRV = std::make_shared<D3D11Texture2D>(Impl->D3D11RHI);
		Impl->DepthSRV->CreateD3D11Texture2D(RenderCore::PF_DepthStencil, ETextureCreateFlags::TexCreate_DepthStencilTargetable , InSizeX, InSizeY);

		ImGui_ImplWin32_Init(InWindowHandle);
	}

	D3D11ViewPort::~D3D11ViewPort()
	{
		Impl = {};
	}

	void* D3D11ViewPort::GetNativeSwapChain() const
	{
		return Impl->SwapChain.get();
	}

	void* D3D11ViewPort::GetNativeBackBufferTexture() const
	{
		return Impl->BackBufferResource.get();
	}

	void* D3D11ViewPort::GetNativeBackBufferRT() const
	{
		return Impl->BackBufferRenderTargetView.get();
	}

	void D3D11ViewPort::SetRenderTarget()
	{
		Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(1, &Impl->BackBufferRenderTargetView, Impl->DepthSRV->GetDSV());
	}

	void D3D11ViewPort::Clear(const core::FLinearColor& Color)
	{
		Impl->D3D11RHI->GetDeviceContext()->ClearRenderTargetView(Impl->BackBufferRenderTargetView.get(), &Color.R);
		Impl->D3D11RHI->GetDeviceContext()->ClearDepthStencilView(Impl->DepthSRV->GetDSV(), D3D11_CLEAR_DEPTH, 1.0f, 0.f);
	}

	void D3D11ViewPort::Present()
	{

		Impl->D3D11RHI->GetDefaultCommandContext()->RHIEndDrawing();
		if (Impl->SwapChain)
		{
			// Present the back buffer to the viewport window.
			uint32_t Flags = 0;
			if ((GetSwapChainFlags() & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) != 0 )
			{
				Flags |= DXGI_PRESENT_ALLOW_TEARING;
			}
			auto Result = Impl->SwapChain->Present(0, Flags);

			//Impl->SwapChain->Present(0, 0);
		}
	}

	void D3D11ViewPort::Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen)
	{
		Impl->D3D11RHI->ClearState();
		Impl->D3D11RHI->GetDeviceContext()->Flush();
		Impl->BackBufferResource = {};
		Impl->BackBufferRenderTargetView = {};

		if (Impl->SizeX != InSizeX || Impl->SizeY != InSizeY )
		{
			Impl->SizeX = InSizeX;
			Impl->SizeY = InSizeY;

			// Resize the swap chain.

			const UINT SwapChainFlags = GetSwapChainFlags();
			const DXGI_FORMAT RenderTargetFormat = GetRenderTargetFormat(Impl->PixelFormat);
			
			// Resize all existing buffers, don't change count
			VERIFYD3D11RESULT(Impl->SwapChain->ResizeBuffers(0, Impl->SizeX, Impl->SizeY, RenderTargetFormat, SwapChainFlags));

			if (bInIsFullscreen)
			{
				DXGI_MODE_DESC BufferDesc = SetupDXGI_MODE_DESC();

				if (FAILED(Impl->SwapChain->ResizeTarget(&BufferDesc)))
				{
					//ResetSwapChainInternal(true);
					VERIFYD3D11RESULT(Impl->SwapChain->ResizeBuffers(0, Impl->SizeX, Impl->SizeY, RenderTargetFormat, SwapChainFlags));

				}
			}
			GetSwapChainSurface();
			Impl->DepthSRV = std::make_shared<D3D11Texture2D>(Impl->D3D11RHI);
			Impl->DepthSRV->CreateD3D11Texture2D(RenderCore::PF_DepthStencil, ETextureCreateFlags::TexCreate_DepthStencilTargetable, InSizeX, InSizeY);
		}
	}

	DXGI_MODE_DESC D3D11ViewPort::SetupDXGI_MODE_DESC() const
	{
		DXGI_MODE_DESC Ret;
		ZeroMemory(&Ret, sizeof(Ret));
		Ret.Width = Impl->SizeX;
		Ret.Height = Impl->SizeY;
		Ret.RefreshRate.Numerator = 60;	
		Ret.RefreshRate.Denominator = 1;	
		Ret.Format = GetRenderTargetFormat(Impl->PixelFormat);
		//Ret.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		//Ret.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		return Ret;
	}

	void D3D11ViewPort::GetSwapChainSurface()
	{
		VERIFYD3D11RESULT(Impl->SwapChain->GetBuffer(0, IID_ID3D11Texture2D, (void**)Impl->BackBufferResource.get_init_ref()));

		D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
		RTVDesc.Format = DXGI_FORMAT_UNKNOWN;
		RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		RTVDesc.Texture2D.MipSlice = 0;
		VERIFYD3D11RESULT(Impl->D3D11RHI->GetDevice()->CreateRenderTargetView(Impl->BackBufferResource.get(), &RTVDesc, Impl->BackBufferRenderTargetView.get_init_ref()));
	}

	uint32_t D3D11ViewPort::GetSwapChainFlags()
	{
		uint32_t SwapChainFlags = GSwapChainFlags;

		// Ensure AllowTearing consistency or ResizeBuffers will fail with E_INVALIDARG
		if ( Impl->bAllowTearing != !!(SwapChainFlags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING))
		{
			SwapChainFlags ^= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		}

		return SwapChainFlags;
	}


}