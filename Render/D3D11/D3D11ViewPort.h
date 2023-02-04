#pragma once
#include "RHI/RHIViewPort.h"
#include "RHIPrivate/D3D11RHIPrivate.h"

namespace RenderCore
{
	struct D3D11ViewPortP;
	class D3D11DynamicRHI;

	class D3D11ViewPort : public RHIViewPort
	{
	public:
		D3D11ViewPort(D3D11DynamicRHI *D3D11RHI, HWND InWindowHandle, uint32_t InSizeX, uint32_t InSizeY);
		virtual ~D3D11ViewPort();

		virtual void* GetNativeSwapChain() const;
		virtual void* GetNativeBackBufferTexture() const;
		virtual void* GetNativeBackBufferRT() const;

		virtual void SetRenderTarget() override;
		virtual void Clear(const core::FLinearColor& Color) override;
		virtual void Present() override;
		virtual void Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen) override;

	private:
		DXGI_MODE_DESC SetupDXGI_MODE_DESC() const;
		void GetSwapChainSurface();
		uint32_t GetSwapChainFlags();

	private:
		std::shared_ptr< D3D11ViewPortP> Impl;
		static uint32_t GSwapChainFlags;
	};
}
