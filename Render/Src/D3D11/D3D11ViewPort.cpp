#include "D3D11/D3D11ViewPort.h"
#include "win/com_ptr.h"
#include "D3D11/D3D11RHI.h"

namespace RenderCore
{
	struct D3D11ViewPortP
	{
		D3D11DynamicRHI* D3D11RHI;
		uint32_t SizeX;
		uint32_t SizeY;
		uint32_t BackBufferCount;
		HWND WindowHandle = nullptr;
	};
	D3D11ViewPort::D3D11ViewPort(D3D11DynamicRHI* D3D11RHI, HWND InWindowHandle, uint32_t InSizeX, uint32_t InSizeY)
		:Data(new D3D11ViewPortP)
	{ 
		Data->D3D11RHI = D3D11RHI;
		Data->WindowHandle = InWindowHandle;
		Data->SizeX = InSizeX;
		Data->SizeY = InSizeY;
	}

	D3D11ViewPort::~D3D11ViewPort()
	{
		Data = {};
	}

}