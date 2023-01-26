#include "D3D11/D3D11ViewPort.h"
#include "win/com_ptr.h"
#include "D3D11/D3D11RHI.h"
#include "core/logger.h"

namespace RenderCore
{
	static DXGI_SWAP_EFFECT GSwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	static DXGI_SCALING GSwapScaling = DXGI_SCALING_STRETCH;
	static uint32_t GSwapChainBufferCount = 1;

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
		Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(1, &Impl->BackBufferRenderTargetView, nullptr);
	}

	void D3D11ViewPort::Clear(const core::FLinearColor& Color)
	{
		Impl->D3D11RHI->GetDeviceContext()->ClearRenderTargetView(Impl->BackBufferRenderTargetView.get(), &Color.R);
	}

	void D3D11ViewPort::Present()
	{
		if (Impl->SwapChain)
		{
			Impl->SwapChain->Present(1, 0);
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

}