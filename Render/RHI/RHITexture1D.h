
#pragma once
#include "RHI/RHIDefinitions.h"

namespace RenderCore
{
	class RHITexture1D
	{
	public:
		RHITexture1D() = default;
		virtual ~RHITexture1D() {}

		virtual bool CreateWithData(EPixelFormat Format, int32_t Flags, int32_t SizeX, void* InBuffer, int RowBytes ) = 0;
		virtual int32_t GetSize() const = 0;
	};
}