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

		virtual bool Create(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool IsMultiSampled, bool CreateDepth) = 0;
		virtual core::vec2i GetSize() const = 0;
		virtual void Bind() = 0;
		virtual void UnBind() = 0;

		virtual std::shared_ptr< RHITexture2D> GetTex() const = 0;
	};
}