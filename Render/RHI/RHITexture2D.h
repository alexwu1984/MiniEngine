#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHITexture2D
	{
	public:
		RHITexture2D() = default;
		virtual ~RHITexture2D() {}

		virtual bool InitTexture(uint32_t format, uint32_t CreateFlags, int32_t width, int32_t height, void* pBuffer = nullptr, int rowBytes = 0) = 0;
	};
}