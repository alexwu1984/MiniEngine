#pragma once
#include "RHI/RHIRenderPass.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHITexture2D.h"
#include "Render/RDGBuilder.h"
#include <vector>

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

	/** Empty render pass: unbind all RTV/DSV (same as SetRenderTarget({}, nullptr)). */
	static void RHICmdListUnbindAllRenderTargets(RenderCore::RHICommandContext& RHICmdList);

	/** Collect pass Inputs/Outputs with non-Unknown access into barrier descriptors (RDG Execute and manual passes). */
	static void AppendPassTextureBarriers(const FRDGPassDescriptor& Pass, std::vector<RenderCore::FRDGTextureBarrierDesc>& Out);
};

inline void FRDGUtils::RHICmdListSetRenderTargetSingleColorNoDepth(RenderCore::RHICommandContext& RHICmdList,
																   const std::shared_ptr<RenderCore::RHITexture2D>& RenderTarget)
{
	if (!RenderTarget)
		return;
	const RenderCore::FRHIRenderPassDesc Desc = RenderCore::FRHIRenderPassDesc::SingleColorNoDepth(RenderTarget);
	RenderCore::RHIBeginRenderPass(RHICmdList, Desc);
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

inline void FRDGUtils::RHICmdListUnbindAllRenderTargets(RenderCore::RHICommandContext& RHICmdList)
{
	RenderCore::RHIBeginRenderPass(RHICmdList, RenderCore::FRHIRenderPassDesc{});
}

inline void FRDGUtils::AppendPassTextureBarriers(const FRDGPassDescriptor& Pass, std::vector<RenderCore::FRDGTextureBarrierDesc>& Out)
{
	auto AppendSlot = [&Out](const FRDGPassResource& R) {
		if (R.Access == FRDGResourceAccess::Unknown || !R.Resolve)
			return;
		std::shared_ptr<RenderCore::RHITexture2D> tex = R.Resolve();
		if (!tex)
			return;
		Out.push_back(RenderCore::FRDGTextureBarrierDesc{std::move(tex), static_cast<RenderCore::FRDGResourceAccess>(R.Access), R.SubresourceIndex});
	};
	for (const FRDGPassResource& In : Pass.Inputs)
		AppendSlot(In);
	for (const FRDGPassResource& O : Pass.Outputs)
		AppendSlot(O);
}
} // namespace Engine
