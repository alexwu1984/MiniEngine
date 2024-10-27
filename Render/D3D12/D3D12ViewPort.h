#pragma once
#include "RHI/RHIViewPort.h"
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "D3D12/D3D12DirectCommandListManager.h"

namespace RenderCore
{
	class D3D12ViewPort : public RHIViewPort, public D3D12AdapterChild
	{
	public:
		D3D12ViewPort(std::weak_ptr<D3D12Adapter> InAdpater, HWND InWindowHandle, uint32_t InSizeX, uint32_t InSizeY);
		virtual ~D3D12ViewPort();

		void Init();
		void Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen) override;

	private:
		// Determine how deep the swapchain should be (based on AFR or not)
		void CalculateSwapChainDepth(int32_t DefaultSwapChainDepth);
		DXGI_MODE_DESC SetupDXGI_MODE_DESC() const;

	private:
		HWND WindowHandle;
		uint32_t SizeX;
		uint32_t SizeY;
		bool bIsFullscreen;
		EPixelFormat PixelFormat;
		bool bIsValid;
		bool bAllowTearing;
		win32::com_ptr<IDXGISwapChain1> SwapChain1;

		DXGI_COLOR_SPACE_TYPE ColorSpace;
		win32::com_ptr<IDXGISwapChain4> SwapChain4;

		/** A fence value used to track the GPU's progress. */
		D3D12Fence Fence;
		uint64_t LastSignaledValue;
		int32_t NumBackBuffers;

		//std::vector<TRefCountPtr<FD3D12Texture2D>> BackBuffers;
	};
}