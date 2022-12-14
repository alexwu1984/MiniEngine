#include "D3D11/D3D11RHI.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11ViewPort.h"

namespace RenderCore
{
	int64_t D3D11GlobalStats::GDedicatedVideoMemory{ 0 };
	int64_t D3D11GlobalStats::GDedicatedSystemMemory{ 0 };
	int64_t D3D11GlobalStats::GSharedSystemMemory{ 0 };
	int64_t D3D11GlobalStats::GTotalGraphicsMemory{ 0 };

	D3D11DynamicRHI::D3D11DynamicRHI()
		:Data(std::make_shared<D3D11DynamicRHIP>())
	{

	}

	D3D11DynamicRHI::~D3D11DynamicRHI()
	{

	}

	void D3D11DynamicRHI::Init()
	{
		InitD3DDevice();
	}

	void D3D11DynamicRHI::Shutdown()
	{

	}

	std::shared_ptr<RHIViewPort> D3D11DynamicRHI::RHICreateViewport(void* WindowHandle, uint32_t SizeX, uint32_t SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
	{
		std::shared_ptr<D3D11ViewPort> ViewPortRHI = std::make_shared<D3D11ViewPort>(this, (HWND)WindowHandle,SizeX,SizeY);
		return ViewPortRHI;
	}

}

