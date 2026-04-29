#pragma once
#include "RHI/RHIViewPort.h"
#include "RHIPrivate/D3D11RHIPrivate.h"

namespace RenderCore
{
	struct D3D11ViewPortPrivate;
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
		virtual void Prepare() override;
		virtual void Clear(const core::FLinearColor& Color) override;
		virtual void Present() override;
		virtual void RHIImGuiRenderDrawData() override;
		virtual void RHISubmitAndPresentFrame() override;
		virtual void Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen) override;
		virtual core::vec2u GetSize()const override;
		std::shared_ptr<RHITexture2D> GetBackBuffer() const override;
	private:
		DXGI_MODE_DESC SetupDXGI_MODE_DESC() const;
		void GetSwapChainSurface();
		uint32_t GetSwapChainFlags() const;

	private:
		D3D11ViewPortPrivate* d_ptr = nullptr;
		static uint32_t GSwapChainFlags;
	};
}
