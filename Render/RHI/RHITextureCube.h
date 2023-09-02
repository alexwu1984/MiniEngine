#pragma once
#include "RHI/RHIDefinitions.h"

namespace RenderCore
{
	class RHITextureCube
	{
	public:
		RHITextureCube() = default;
		virtual ~RHITextureCube() {}

		virtual bool CreateD3D11TextureCube(EPixelFormat Format, int32_t Flags, int32_t SizeX, int32_t SizeY) = 0;
		virtual bool IsMultisampled() const = 0;
		virtual core::vec2i GetSize() const = 0;
	};
}