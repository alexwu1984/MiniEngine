#pragma once
#include "RHI/RHIDefinitions.h"

namespace RenderCore
{
	class RHITexture2D
	{
	public:
		RHITexture2D() = default;
		virtual ~RHITexture2D() {}

		virtual bool CreateTexture2D(EPixelFormat Format, int32_t Flags, int32_t SizeX, int32_t SizeY,int32_t SizeZ=1, 
			uint32_t NumMips = 1, void* InBuffer = nullptr, int RowBytes = 0) = 0;
		virtual bool CreateFromFile(const std::wstring& FileName) = 0;
		virtual bool CreateHDRFromFile(const std::wstring& FileName) = 0;
		virtual bool IsMultisampled() const = 0;
		virtual core::vec2i GetSize() const = 0;
		virtual uint32_t GetNumMips() const = 0;
		virtual EPixelFormat GetPixelFormat() const = 0;
	};
}