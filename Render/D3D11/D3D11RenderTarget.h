#pragma once
#include "RHI/RHIRenderTarget.h"
#include "RHIPrivate/D3D11RHIDeclare.h"


namespace RenderCore
{
	struct D3D11RenderTargetP;
	class D3D11DynamicRHI;

	class D3D11RenderTarget : public RHIRenderTarget
	{
	public:
		D3D11RenderTarget(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11RenderTarget();

		virtual bool CreateWithTexture(std::shared_ptr< RHITexture2D> Tex, bool CreateDepth) override;
		virtual bool Create(EPixelFormat Format, int32_t SizeX, int32_t SizeY,bool IsMultiSampled, bool CreateDepth) override;

		virtual void Bind() override;
		virtual void UnBind() override;

		ID3D11Texture2D* GetNativeTex() const;
		ID3D11RenderTargetView* GetRTV() const;
		ID3D11ShaderResourceView* GetSRV() const;
		ID3D11DepthStencilView* GetDSV() const;

	private:
		std::shared_ptr< D3D11RenderTargetP> Data;
	};
}