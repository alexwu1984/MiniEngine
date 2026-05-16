#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHITexture2D;

	/** RDG resource access → pass-begin resource states (D3D12). */
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

	/** Whole-resource subresource index for barriers / FRDGPassResource (UINT32_MAX). */
	inline constexpr uint32_t FRDGAllSubresources = UINT32_MAX;

	struct FRDGTextureBarrierDesc
	{
		std::shared_ptr<RHITexture2D> Texture;
		FRDGResourceAccess Access = FRDGResourceAccess::Unknown;
		uint32_t SubresourceIndex = FRDGAllSubresources;
	};
}
