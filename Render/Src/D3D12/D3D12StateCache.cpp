#include "D3D12/D3D12StateCache.h"
#include "D3D12/D3D12Shaders.h"
#include "common/crc.h"

namespace RenderCore
{

	FD3D12StateCache::FD3D12StateCache(std::weak_ptr<FD3D12Device> InParent)
		:FD3D12DeviceChild(InParent)
	{

	}

	void FD3D12StateCache::SetVertexShader(std::shared_ptr<FD3D12VertexShader> InVertexShader)
	{
		if (InVertexShader)
		{
			VertexShaders.insert({ InVertexShader->Hash, InVertexShader });
		}
	}

	void FD3D12StateCache::SetPixelShader(std::shared_ptr<FD3D12PixelShader> InPixelShader)
	{
		if (InPixelShader)
		{
			PixelShaders.insert({ InPixelShader->Hash, InPixelShader });
		}
	}

}