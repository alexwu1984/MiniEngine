#include "D3D11/D3D11RHI.h"
#include "RHIPrivate/D3D11RHIPrivate.h"

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

}

