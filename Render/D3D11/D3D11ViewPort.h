#pragma once
#include "RHI/RHIViewPort.h"

namespace RenderCore
{
	struct D3D11ViewPortP;
	class D3D11DynamicRHI;

	class D3D11ViewPort : public RHIViewPort
	{
	public:
		D3D11ViewPort(D3D11DynamicRHI *D3D11RHI, HWND InWindowHandle, uint32_t InSizeX, uint32_t InSizeY);
		virtual ~D3D11ViewPort();

	private:
		std::shared_ptr< D3D11ViewPortP> Data;
	};
}
