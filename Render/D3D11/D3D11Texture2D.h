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

		virtual bool InitTexture(uint32_t format, uint32_t BindFlags, int32_t width, int32_t height, void* pBuffer = nullptr, int rowBytes = 0, bool bMultSample = false) override;

	private:
		std::shared_ptr< D3D11Texture2DP> Data;
	};
}