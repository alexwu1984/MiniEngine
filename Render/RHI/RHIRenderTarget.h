#pragma once
#include "core/inc.h"
#include "RHI/RHIDefinitions.h"
#include "math/vector4.h"

namespace RenderCore
{
	class RHITexture2D;
	class RHIRenderTarget
	{
	public:
		RHIRenderTarget() = default;
		virtual ~RHIRenderTarget() {}

		virtual bool CreateWithTexture( std::shared_ptr< RHITexture2D> Tex, bool CreateDepth) = 0;
		virtual bool Create(EPixelFormat Format, int32_t SizeX, int32_t SizeY, bool CreateDepth) = 0;

		virtual void Bind() = 0;
		virtual void UnBind() = 0;
	};
}