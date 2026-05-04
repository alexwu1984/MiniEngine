#pragma once
#include "RHI/RHICommandContext.h"
#include "RHI/RHITexture2D.h"

namespace Engine
{
/** Raster helpers: bind render targets and viewport before graphics PSO (OM consistent with pipeline). */
struct FRDGUtils
{
	static void RHICmdListSetRenderTargetSingleColorNoDepth(RenderCore::RHICommandContext& RHICmdList,
														   const std::shared_ptr<RenderCore::RHITexture2D>& RenderTarget);

	static void RHICmdListSetViewportSize(RenderCore::RHICommandContext& RHICmdList, int32_t SizeX, int32_t SizeY);

	static void RHICmdListSetViewportFromTexture(RenderCore::RHICommandContext& RHICmdList,
												 const std::shared_ptr<RenderCore::RHITexture2D>& Texture);
};

inline void FRDGUtils::RHICmdListSetRenderTargetSingleColorNoDepth(RenderCore::RHICommandContext& RHICmdList,
																   const std::shared_ptr<RenderCore::RHITexture2D>& RenderTarget)
{
	if (RenderTarget)
		RHICmdList.SetRenderTarget(RenderTarget, nullptr);
}

inline void FRDGUtils::RHICmdListSetViewportSize(RenderCore::RHICommandContext& RHICmdList, int32_t SizeX, int32_t SizeY)
{
	if (SizeX > 0 && SizeY > 0)
		RHICmdList.SetViewPort(0, 0, SizeX, SizeY);
}

inline void FRDGUtils::RHICmdListSetViewportFromTexture(RenderCore::RHICommandContext& RHICmdList,
														 const std::shared_ptr<RenderCore::RHITexture2D>& Texture)
{
	if (!Texture)
		return;
	const core::vec2i Sz = Texture->GetSize();
	RHICmdListSetViewportSize(RHICmdList, Sz.x, Sz.y);
}
} // namespace Engine
