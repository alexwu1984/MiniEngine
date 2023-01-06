#pragma once
#include "RHI/RHITexture2D.h"

namespace RenderCore
{
	struct D3D11Texture2DP;
	class D3D11DynamicRHI;

	class D3D11Texture2D : public RHITexture2D
	{
	public:
		D3D11Texture2D(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11Texture2D();

		virtual bool InitTexture(uint32_t Format, uint32_t CreateFlags, int32_t SizeX, int32_t SizeY, void* pBuffer = nullptr, int rowBytes = 0) override;

	private:
		std::shared_ptr< D3D11Texture2DP> Data;
	};
}