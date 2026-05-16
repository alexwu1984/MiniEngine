#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHITexture2D;
	class RHITextureCube;
	class RHIStructuredBuffer;

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
		/** When non-null, barriers apply to this cubemap resource instead of Texture (Texture may stay null). */
		std::shared_ptr<RHITextureCube> TextureCube;
	};

	struct FRDGStructuredBufferBarrierDesc
	{
		std::shared_ptr<RHIStructuredBuffer> Buffer;
		FRDGResourceAccess Access = FRDGResourceAccess::Unknown;
		/** When Access == SRV: true → NON_PIXEL_SHADER_RESOURCE (compute), false → PIXEL_SHADER_RESOURCE. Ignored for UAV. */
		bool bNonPixelShaderSrv = false;
	};
}
