#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>

namespace RenderCore
{
	class RHITexture2D;

	/** RDG texture access (maps to D3D12 resource states at pass boundaries; UE RDG-style Epilogue/Begin). */
	enum class FRDGResourceAccess : uint8_t
	{
		Unknown = 0,
		SRV,
		UAV,
		RTV,
		DSV,
		CopySrc,
		CopyDst,
	};

	enum class ERDGPassQueue : uint8_t
	{
		Graphics = 0,
		AsyncCompute = 1,
		Copy = 2,
	};

	struct FRDGTextureBarrierDesc
	{
		std::shared_ptr<RHITexture2D> Texture;
		FRDGResourceAccess Access = FRDGResourceAccess::Unknown;
		uint32_t SubresourceIndex = 0xFFFFFFFFu;
	};
}
