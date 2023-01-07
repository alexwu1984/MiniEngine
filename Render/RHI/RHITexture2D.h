#pragma once
#include "RHI/RHIDefinitions.h"

namespace RenderCore
{
	class RHITexture2D
	{
	public:
		RHITexture2D() = default;
		virtual ~RHITexture2D() {}

		virtual bool CreateWithData(EPixelFormat Format, ETextureCreateFlags Flags, int32_t SizeX, int32_t SizeY, void* InBuffer = nullptr, int RowBytes = 0) = 0;
		virtual bool CreateFromFile(const std::wstring& FileName) = 0;
		virtual bool CreateHDRFromFile(const std::wstring& FileName) = 0;
		virtual core::vec2i GetSize() const = 0;
	};
}