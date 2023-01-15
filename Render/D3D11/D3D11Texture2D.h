#pragma once
#include "RHI/RHITexture2D.h"
#include "RHIPrivate/D3D11RHIDeclare.h"

namespace RenderCore
{
	struct D3D11Texture2DP;
	class D3D11DynamicRHI;

	class D3D11Texture2D : public RHITexture2D
	{
	public:
		D3D11Texture2D(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11Texture2D();

		virtual bool CreateWithData(EPixelFormat Format, ETextureCreateFlags Flags, int32_t SizeX, int32_t SizeY, void* InBuffer = nullptr, int RowBytes = 0) override;
		virtual bool CreateFromFile(const std::wstring& FileName) override;
		virtual bool CreateHDRFromFile(const std::wstring& FileName) override;
		virtual core::vec2i GetSize() const;

		ID3D11Texture2D* GetNativeTex() const;
		ID3D11RenderTargetView* GetRTV() const;
		ID3D11ShaderResourceView* GetSRV() const;
		ID3D11DepthStencilView* GetDSV() const;

	private:
		std::shared_ptr< D3D11Texture2DP> Impl;
	};
}