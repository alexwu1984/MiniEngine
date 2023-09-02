#pragma once
#include "RHI/RHITextureCube.h"
#include "RHIPrivate/D3D11RHIDeclare.h"

namespace RenderCore
{
	struct D3D11TextureCubePrivate;
	class D3D11DynamicRHI;

	class D3D11TextureCube : public RHITextureCube
	{
	public:
		D3D11TextureCube(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11TextureCube();

		virtual bool CreateD3D11TextureCube(EPixelFormat Format, int32_t Flags, int32_t SizeX, int32_t SizeY);
		virtual bool IsMultisampled() const;
		virtual core::vec2i GetSize() const;

	private:
		D3D11TextureCubePrivate* d_ptr = nullptr;
	};
}