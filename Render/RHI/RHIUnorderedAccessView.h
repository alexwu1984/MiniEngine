#pragma once
#include "RHI/RHIDefinitions.h"

namespace RenderCore
{
	class RHITexture2D;

	class RHIUnorderedAccessView
	{
	public:
		RHIUnorderedAccessView() = default;
		virtual ~RHIUnorderedAccessView() {}

		virtual bool CreateFromTexture(std::shared_ptr<RHITexture2D> Tex2D,uint32_t MipLevel) = 0;
		virtual std::shared_ptr<RHITexture2D> GetTextre2D() const = 0;
	};
}