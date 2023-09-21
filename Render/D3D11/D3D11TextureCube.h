#pragma once
#include "RHI/RHITextureCube.h"
#include "RHIPrivate/D3D11RHIDeclare.h"
#include "win/com_ptr.h"
#include <d3d11.h>

namespace RenderCore
{
	struct D3D11TextureCubePrivate;
	class D3D11DynamicRHI;
	class D3D11Texture2D;

	class D3D11TextureCube : public RHITextureCube
	{
	public:
		D3D11TextureCube(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11TextureCube();

		virtual bool CreateD3D11TextureCube(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips,bool CreateDepth) override;
		virtual core::vec2i GetSize() const override;

		ID3D11Texture2D* GetNativeTex() const;
		std::map < uint32_t, std::vector< win32::com_ptr <ID3D11RenderTargetView>>> GetRTVS() const;
		ID3D11ShaderResourceView* GetSRV() const;
		std::shared_ptr<D3D11Texture2D> GetDepthTex() const;

	private:
		D3D11TextureCubePrivate* d_ptr = nullptr;
	};
}