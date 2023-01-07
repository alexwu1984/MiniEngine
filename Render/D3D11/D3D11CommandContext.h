#pragma once
#include "RHI/RHICommandContext.h"

namespace RenderCore
{
	class D3D11DynamicRHI;
	struct D3D11CommandContextP;

	class D3D11CommandContext : public RHICommandContext
	{
	public:
		D3D11CommandContext(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11CommandContext();

		virtual void SetViewPort(int32_t TopLeftX, int32_t TopLeftY, int32_t SizeX, int32_t SizeY) override;
		virtual void SetRenderTarget(std::shared_ptr<RHITexture2D> Tex, std::shared_ptr< RHITexture2D> Depth) override;
		virtual void SetRenderTarget(std::shared_ptr< RHIRenderTarget> RenderTarget) override;
		virtual void Clear(std::shared_ptr< RHIRenderTarget> RenderTarget,const math::Vector4 Color, float Depth = 1.0f, uint8_t Stencil = 0) override;

	private:
		std::shared_ptr< D3D11CommandContextP> Data;
	};
}