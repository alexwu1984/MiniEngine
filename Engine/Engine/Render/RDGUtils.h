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

	/** One FRDG pass slot (named resolve + access); no-op if Tex is null or Access is Unknown. */
	static void AddPassTexture2D(std::vector<FRDGPassResource>& Slots, const char* Name, const std::shared_ptr<RenderCore::RHITexture2D>& Tex,
								 FRDGResourceAccess Access);

	static void AddPassTextureSrv(std::vector<FRDGPassResource>& Inputs, const char* Name, const std::shared_ptr<RenderCore::RHITexture2D>& Tex)
	{
		AddPassTexture2D(Inputs, Name, Tex, FRDGResourceAccess::SRV);
	}

	static void AddPassTextureRtv(std::vector<FRDGPassResource>& Outputs, const char* Name, const std::shared_ptr<RenderCore::RHITexture2D>& Tex)
	{
		AddPassTexture2D(Outputs, Name, Tex, FRDGResourceAccess::RTV);
	}

	/** Append a 2D / cube SRV transition directly on FRHIRenderPassDesc (no FRDG pass IO). */
	static void AppendDeclaredTexture2DSrv(std::vector<RenderCore::FRDGTextureBarrierDesc>& Out, const std::shared_ptr<RenderCore::RHITexture2D>& Tex);
	static void AppendDeclaredTextureCubeSrv(std::vector<RenderCore::FRDGTextureBarrierDesc>& Out, const std::shared_ptr<RenderCore::RHITextureCube>& Cube);

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

	inline void FRDGUtils::AddPassTexture2D(std::vector<FRDGPassResource>& Slots, const char* Name, const std::shared_ptr<RenderCore::RHITexture2D>& Tex,
										   FRDGResourceAccess Access)
	{
		if (!Tex || Access == FRDGResourceAccess::Unknown)
			return;
		std::shared_ptr<RenderCore::RHITexture2D> Cap = Tex;
		Slots.push_back({
			Name ? std::string(Name) : std::string("Texture"),
			[Cap]() { return Cap; },
			true,
			Access,
		});
	}

	inline void FRDGUtils::AppendDeclaredTexture2DSrv(std::vector<RenderCore::FRDGTextureBarrierDesc>& Out,
														const std::shared_ptr<RenderCore::RHITexture2D>& Tex)
	{
		if (!Tex)
			return;
		Out.push_back({ Tex, RenderCore::FRDGResourceAccess::SRV, RenderCore::FRDGAllSubresources, {} });
	}

	inline void FRDGUtils::AppendDeclaredTextureCubeSrv(std::vector<RenderCore::FRDGTextureBarrierDesc>& Out,
														  const std::shared_ptr<RenderCore::RHITextureCube>& Cube)
	{
		if (!Cube)
			return;
		RenderCore::FRDGTextureBarrierDesc B{};
		B.TextureCube = Cube;
		B.Access = RenderCore::FRDGResourceAccess::SRV;
		B.SubresourceIndex = RenderCore::FRDGAllSubresources;
		Out.push_back(std::move(B));
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
				core::LOG(core::log_war,
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
	for (const auto& Slot : SrvTextures)
		AddPassTextureSrv(Tmp.Inputs, Slot.first, Slot.second);
	if (RtvTexture)
		AddPassTextureRtv(Tmp.Outputs, "FullscreenRT", RtvTexture);
	FRDGUtils::AppendPassTextureBarriers(Tmp, Om.DeclaredTextureBarriers);
}
} // namespace Engine
