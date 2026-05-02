#pragma once
#include "RHI/RHITexture2D.h"
#include "RHIPrivate/D3D11RHIDeclare.h"
#include "win/com_ptr.h"

namespace RenderCore
{
	struct D3D11Texture2DPrivate;
	class D3D11DynamicRHI;

	std::shared_ptr<uint8_t> GetImageData(const std::wstring& path,int32_t &X,int32_t &Y);

	class D3D11Texture2D : public RHITexture2D
	{
	public:
		D3D11Texture2D(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11Texture2D();

		virtual bool CreateTexture2D(EPixelFormat Format, int32_t Flags, int32_t SizeX, int32_t SizeY, int32_t SizeZ=1, 
										uint32_t NumMips=1, void* InBuffer = nullptr, int RowBytes = 0) override;
		bool CreateTexture2D(EPixelFormat Format, int32_t Flags, int32_t SizeX, int32_t SizeY, int32_t SizeZ, bool bCubeTexture, uint32_t NumMips, void* InBuffer, size_t RowBytes);
		virtual bool CreateFromFile(const std::wstring& FileName) override;
		virtual bool CreateHDRFromFile(const std::wstring& FileName) override;
		virtual bool IsMultisampled() const override;
		virtual core::vec2i GetSize() const;
		virtual uint32_t GetNumMips() const;
		virtual EPixelFormat GetPixelFormat() const override;

		ID3D11Texture2D* GetNativeTex() const;
		ID3D11RenderTargetView* GetRTV() const;
		std::map < uint32_t, std::vector< win32::com_ptr <ID3D11RenderTargetView>>> GetRTVS() const;
		std::map < uint32_t, std::vector< win32::com_ptr <ID3D11RenderTargetView>>>& GetRTVS();
		ID3D11ShaderResourceView* GetSRV() const;
		ID3D11DepthStencilView* GetDSV() const;

	private:
		D3D11Texture2DPrivate* d_ptr = nullptr;
	};
}