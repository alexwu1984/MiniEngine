#pragma once
#include "RHI/RHIRenderPass.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHITextureCube.h"
#include "Render/RDGBuilder.h"

#ifdef _DEBUG
#include "core/logger.h"
#include <unordered_map>
#endif

namespace Engine
{
/** Raster helpers (viewport / OM unbind / barrier scratch from RDG-style pass slots). */
struct FRDGUtils
{
	static void RHICmdListSetRenderTargetSingleColorNoDepth(RenderCore::RHICommandContext& RHICmdList,
														   const std::shared_ptr<RenderCore::RHITexture2D>& RenderTarget);

	static void RHICmdListSetViewportSize(RenderCore::RHICommandContext& RHICmdList, int32_t SizeX, int32_t SizeY);

	static void RHICmdListSetViewportFromTexture(RenderCore::RHICommandContext& RHICmdList,
												 const std::shared_ptr<RenderCore::RHITexture2D>& Texture);

	/** Empty render pass: unbind all RTV/DSV (same as SetRenderTarget({}, nullptr)). */
	static void RHICmdListUnbindAllRenderTargets(RenderCore::RHICommandContext& RHICmdList);

	static void RHICmdListDeclarePixelSamplingSrvs(RenderCore::RHICommandContext& RHICmdList,
												  std::initializer_list<std::shared_ptr<RenderCore::RHITexture2D>> Texes);
	static void RHICmdListDeclarePixelSamplingSrvCube(RenderCore::RHICommandContext& RHICmdList,
													  const std::shared_ptr<RenderCore::RHITextureCube>& Cube);
	static void RHICmdListDeclareComputeReadableSrvs(RenderCore::RHICommandContext& RHICmdList,
												   std::initializer_list<std::shared_ptr<RenderCore::RHITexture2D>> Texes);
	static void RHICmdListDeclareTextureUavs(RenderCore::RHICommandContext& RHICmdList,
											 std::initializer_list<std::shared_ptr<RenderCore::RHITexture2D>> Texes);

	static void AppendPassTextureBarriers(const FRDGPassDescriptor& Pass, std::vector<RenderCore::FRDGTextureBarrierDesc>& Out);

	/** Merge fullscreen-style SRV inputs + optional RTV output barriers into Om (appends to Om.DeclaredTextureBarriers). */
	static void AppendFullscreenDeclaredTextureBarriers(RenderCore::FRHIRenderPassDesc& Om,
														std::initializer_list<std::pair<const char*, std::shared_ptr<RenderCore::RHITexture2D>>> SrvTextures,
														std::shared_ptr<RenderCore::RHITexture2D> RtvTexture);
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

	inline void FRDGUtils::RHICmdListDeclarePixelSamplingSrvs(RenderCore::RHICommandContext& RHICmdList,
															 std::initializer_list<std::shared_ptr<RenderCore::RHITexture2D>> Texes)
	{
		std::vector<RenderCore::FRDGTextureBarrierDesc> Barriers;
		for (const auto& T : Texes)
			if (T)
				Barriers.push_back({ T, RenderCore::FRDGResourceAccess::SRV, RenderCore::FRDGAllSubresources, {} });
		if (!Barriers.empty())
			RHICmdList.RHIRenderPassApplyDeclaredTextureBarriers(Barriers.data(), Barriers.size(), RenderCore::ERDGPassQueue::Graphics);
	}

	inline void FRDGUtils::RHICmdListDeclarePixelSamplingSrvCube(RenderCore::RHICommandContext& RHICmdList,
																 const std::shared_ptr<RenderCore::RHITextureCube>& Cube)
	{
		if (!Cube)
			return;
		RenderCore::FRDGTextureBarrierDesc Desc{};
		Desc.TextureCube = Cube;
		Desc.Access = RenderCore::FRDGResourceAccess::SRV;
		Desc.SubresourceIndex = RenderCore::FRDGAllSubresources;
		RHICmdList.RHIRenderPassApplyDeclaredTextureBarriers(&Desc, 1, RenderCore::ERDGPassQueue::Graphics);
	}

	inline void FRDGUtils::RHICmdListDeclareComputeReadableSrvs(RenderCore::RHICommandContext& RHICmdList,
																std::initializer_list<std::shared_ptr<RenderCore::RHITexture2D>> Texes)
	{
		std::vector<RenderCore::FRDGTextureBarrierDesc> Barriers;
		for (const auto& T : Texes)
			if (T)
				Barriers.push_back({ T, RenderCore::FRDGResourceAccess::SRV, RenderCore::FRDGAllSubresources, {} });
		if (!Barriers.empty())
			RHICmdList.RHIRenderPassApplyDeclaredTextureBarriers(Barriers.data(), Barriers.size(), RenderCore::ERDGPassQueue::AsyncCompute);
	}

	inline void FRDGUtils::RHICmdListDeclareTextureUavs(RenderCore::RHICommandContext& RHICmdList,
														std::initializer_list<std::shared_ptr<RenderCore::RHITexture2D>> Texes)
	{
		std::vector<RenderCore::FRDGTextureBarrierDesc> Barriers;
		for (const auto& T : Texes)
			if (T)
				Barriers.push_back({ T, RenderCore::FRDGResourceAccess::UAV, RenderCore::FRDGAllSubresources, {} });
		if (!Barriers.empty())
			RHICmdList.RHIRenderPassApplyDeclaredTextureBarriers(Barriers.data(), Barriers.size(), RenderCore::ERDGPassQueue::Graphics);
	}

	inline void FRDGUtils::AppendPassTextureBarriers(const FRDGPassDescriptor& Pass, std::vector<RenderCore::FRDGTextureBarrierDesc>& Out)
	{
#ifdef _DEBUG
		std::unordered_map<uint64_t, FRDGResourceAccess> RDG_DebugBarrierSlotsSeen;
#endif
		auto AppendSlot = [&](const FRDGPassResource& R) {
			if (R.Access == FRDGResourceAccess::Unknown || !R.Resolve)
				return;
			std::shared_ptr<RenderCore::RHITexture2D> tex = R.Resolve();
			if (!tex)
				return;
#ifdef _DEBUG
			const uint64_t Key =
				uint64_t(reinterpret_cast<uintptr_t>(tex.get())) ^ (uint64_t(R.SubresourceIndex + 1u) << 17);
			auto Ins = RDG_DebugBarrierSlotsSeen.try_emplace(Key, R.Access);
			if (!Ins.second && Ins.first->second != R.Access)
				core::LOG(core::log_warning,
						  L"AppendPassTextureBarriers: conflicting accesses on same texture/subresource (check pass IO).");
#endif
			Out.push_back(RenderCore::FRDGTextureBarrierDesc{std::move(tex), static_cast<RenderCore::FRDGResourceAccess>(R.Access), R.SubresourceIndex, {}});
		};
		for (const FRDGPassResource& In : Pass.Inputs)
			AppendSlot(In);
		for (const FRDGPassResource& O : Pass.Outputs)
			AppendSlot(O);
	}

inline void FRDGUtils::AppendFullscreenDeclaredTextureBarriers(
	RenderCore::FRHIRenderPassDesc& Om,
	std::initializer_list<std::pair<const char*, std::shared_ptr<RenderCore::RHITexture2D>>> SrvTextures,
	std::shared_ptr<RenderCore::RHITexture2D> RtvTexture)
{
	FRDGPassDescriptor Tmp{};
	using A = FRDGResourceAccess;
	for (const auto& Slot : SrvTextures)
	{
		if (!Slot.second)
			continue;
		std::shared_ptr<RenderCore::RHITexture2D> Cap = Slot.second;
		Tmp.Inputs.push_back({ Slot.first ? std::string(Slot.first) : std::string("Srv"), [Cap]() { return Cap; }, true, A::SRV });
	}
	if (RtvTexture)
	{
		std::shared_ptr<RenderCore::RHITexture2D> Rt = RtvTexture;
		Tmp.Outputs.push_back({ "FullscreenRT", [Rt]() { return Rt; }, true, A::RTV });
	}
	FRDGUtils::AppendPassTextureBarriers(Tmp, Om.DeclaredTextureBarriers);
}
} // namespace Engine
