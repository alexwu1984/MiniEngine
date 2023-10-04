#pragma once
#include "RHI/RHIDefinitions.h"

namespace RenderCore
{
	class RHITextureCube
	{
	public:
		RHITextureCube() = default;
		virtual ~RHITextureCube() {}

		virtual bool CreateD3D11TextureCube(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool CreateDepth) = 0;
		virtual core::vec2i GetSize() const = 0;
		virtual uint32_t GetNumMips() const = 0;
	};
}