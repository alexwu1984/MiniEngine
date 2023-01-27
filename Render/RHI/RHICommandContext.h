#pragma once
#include "RHI/RHIDefinitions.h"
#include "core/color.h"

namespace RenderCore
{
	class RHITexture2D;
	class RHIRenderTarget;
	class RHIPixelShader;
	class RHISamplerState;
	class RHIRasterizerState;
	class RHIBlendState;
	class RHIDepthStencilState;

	class RHICommandContext
	{
	public:
		RHICommandContext() = default;
		virtual ~RHICommandContext() {}

		virtual void SetViewPort(int32_t TopLeftX, int32_t TopLeftY, int32_t SizeX, int32_t SizeY) = 0;
		virtual void SetRenderTarget(std::shared_ptr< RHITexture2D> Tex, std::shared_ptr< RHITexture2D> Depth) = 0;
		virtual void SetRenderTarget(std::shared_ptr< RHIRenderTarget> RenderTarget) = 0;
		virtual void Clear(std::shared_ptr< RHIRenderTarget> RenderTarget, const core::FLinearColor& Color, float Depth = 1.0f, uint8_t Stencil = 0) = 0;

		virtual void RHISetShaderSampler(EShaderFrequency ShaderType, uint32_t SamplerIndex, std::shared_ptr< RHISamplerState> NewState) = 0;
		virtual void RHISetRasterizerState(std::shared_ptr<RHIRasterizerState> NewStateRHI) = 0;
		virtual void RHISetBlendState(std::shared_ptr<RHIBlendState> NewState, const core::FLinearColor& BlendFactor) = 0;
		virtual void RHISetBlendFactor(const core::FLinearColor& BlendFactor)  = 0;
		virtual void RHISetDepthStencilState(std::shared_ptr< RHIDepthStencilState> NewState, uint32_t StencilRef) =0;
		virtual void RHISetStencilRef(uint32_t StencilRef)  = 0;
	};
}