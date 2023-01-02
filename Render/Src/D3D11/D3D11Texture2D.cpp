#include "D3D11/D3D11Texture2D.h"
#include "RHI/RHIDefinitions.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11RHI.h"
#include "DirectXTex/DirectXTex.h"

namespace RenderCore
{
	struct D3D11Texture2DP
	{
		D3D11DynamicRHI* D3D11RHI;
	};

	D3D11Texture2D::D3D11Texture2D(D3D11DynamicRHI* D3D11RHI)
		:Data(std::make_shared<D3D11Texture2DP>())
	{
		Data->D3D11RHI = D3D11RHI;
	}

	D3D11Texture2D::~D3D11Texture2D()
	{

	}

	bool D3D11Texture2D::InitTexture(uint32_t format, uint32_t BindFlags, int32_t width, int32_t height, void* pBuffer /*= nullptr*/, int rowBytes /*= 0*/, bool bMultSample /*= false*/)
	{
		return false;
	}

}