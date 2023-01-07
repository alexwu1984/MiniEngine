#pragma once
#include "RHI/RHIDefinitions.h"
#include "math/vector4.h"

namespace RenderCore
{
	class RHITexture2D;
	class RHIRenderTarget;

	class RHICommandContext
	{
	public:
		RHICommandContext() = default;
		virtual ~RHICommandContext() {}

		virtual void SetViewPort(int32_t TopLeftX, int32_t TopLeftY, int32_t SizeX, int32_t SizeY) = 0;
		virtual void SetRenderTarget(std::shared_ptr< RHITexture2D> Tex, std::shared_ptr< RHITexture2D> Depth) = 0;
		virtual void SetRenderTarget(std::shared_ptr< RHIRenderTarget> RenderTarget) = 0;
		virtual void Clear(std::shared_ptr< RHIRenderTarget> RenderTarget, const math::Vector4 Color, float Depth = 1.0f, uint8_t Stencil = 0) = 0;
	};
}