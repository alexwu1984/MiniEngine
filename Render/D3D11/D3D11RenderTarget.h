#pragma once
#include "RHI/RHIRenderTarget.h"
#include "RHIPrivate/D3D11RHIDeclare.h"
#include "win/com_ptr.h"

namespace RenderCore
{
	struct D3D11RenderTargetPrivate;
	class D3D11DynamicRHI;

	class D3D11RenderTarget : public RHIRenderTarget
	{
	public:
		D3D11RenderTarget(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11RenderTarget();

		virtual bool Create(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips,bool IsMultiSampled, bool CreateDepth) override;
		virtual core::vec2i GetSize() const override;
		virtual void Bind() override;
		virtual void UnBind() override;

		ID3D11Texture2D* GetNativeTex() const;
		ID3D11RenderTargetView* GetRTV() const;
		ID3D11ShaderResourceView* GetSRV() const;
		ID3D11DepthStencilView* GetDSV() const;

		std::map < uint32_t, std::vector< win32::com_ptr <ID3D11RenderTargetView>>> GetRTVS() const;
		std::map < uint32_t, std::vector< win32::com_ptr <ID3D11RenderTargetView>>>& GetRTVS();

		virtual std::shared_ptr< RHITexture2D> GetTex() const;

	private:
		D3D11RenderTargetPrivate* d_ptr = nullptr;
	};
}