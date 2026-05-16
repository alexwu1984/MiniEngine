#pragma once
#include "RHI/RHIDefinitions.h"
#include "tinygltf/json.h"

namespace RenderCore
{
	class DynamicRHI;
	class RHITexture2D;
	class RHIUnorderedAccessView;
	class RHIRenderTarget;
}

namespace Engine
{
	// Transient RT / UAV / RTV pool keyed by format + dimensions (+ create flags).
	// Effects acquire on draw and release back on resize or when superseded; reuse cuts Init/Resize churn.
	// Future: alias disjoint lifetimes (PlacedResource); today whole-resource pooling only.
	class RenderTexturePool
	{
	public:
		static RenderTexturePool& Get();

		template <class T>
		struct PoolEntry
		{
			std::shared_ptr<T> Resource;
			uint64_t LastUsedFrame = 0;
		};

		// Evict entries unused for a while to keep memory stable.
		static constexpr uint64_t kEvictAfterFrames = 600; // ~5s at 120fps

		std::shared_ptr<RenderCore::RHITexture2D> AcquireTexture2D(
			RenderCore::DynamicRHI* RHI, RenderCore::EPixelFormat Format, int32_t CreateFlags,
			int32_t Width, int32_t Height, uint32_t NumMips = 1);

		void ReleaseTexture2D(RenderCore::EPixelFormat Format, int32_t CreateFlags,
			int32_t Width, int32_t Height, uint32_t NumMips, std::shared_ptr<RenderCore::RHITexture2D>&& Tex);

		std::shared_ptr<RenderCore::RHIUnorderedAccessView> AcquireUAV(
			RenderCore::DynamicRHI* RHI, RenderCore::EPixelFormat Format, int32_t Width, int32_t Height);

		void ReleaseUAV(RenderCore::EPixelFormat Format, int32_t Width, int32_t Height,
			std::shared_ptr<RenderCore::RHIUnorderedAccessView>&& Uav);

		std::shared_ptr<RenderCore::RHIRenderTarget> AcquireRenderTarget(
			RenderCore::DynamicRHI* RHI, RenderCore::EPixelFormat Format,
			int32_t Width, int32_t Height, uint32_t NumMips, bool IsMultiSampled, bool CreateDepth);

		void ReleaseRenderTarget(RenderCore::EPixelFormat Format, int32_t Width, int32_t Height,
			uint32_t NumMips, bool IsMultiSampled, bool CreateDepth,
			std::shared_ptr<RenderCore::RHIRenderTarget>&& Rt);

		// FRDG: use FRDGCompileParameters::bLogRenderTexturePoolStats after Execute for stats; RHI SetFrameCallbacks drive Begin/EndFrame.
		void BeginFrame();
		void EndFrame();
		void Clear();

		struct Stats
		{
			uint64_t FrameCounter = 0;
			std::size_t FreeTex2D = 0;
			std::size_t FreeUav = 0;
			std::size_t FreeRt = 0;
			uint64_t EstimatedBytesFree = 0;
			uint64_t BudgetBytes = 0;
		};

		Stats GetStats() const;
		void SetBudgetBytes(uint64_t InBudgetBytes);
		uint64_t GetBudgetBytes() const;

		/** Optional JSON root: { "RenderTexturePool": { "BudgetMB": 512, "BudgetBytes": ... } } */
		void ApplyConfigFromJson(const nlohmann::json& Root);

	private:
		RenderTexturePool() = default;

		static constexpr std::size_t kMaxFreePerKey = 8;
		static constexpr uint64_t kDefaultBudgetBytes = 512ull * 1024ull * 1024ull; // 512MB

		struct Tex2DKey
		{
			RenderCore::EPixelFormat Format;
			int32_t Flags;
			int32_t W, H;
			uint32_t NumMips;
			bool operator<(const Tex2DKey& o) const;
		};

		struct UavKey
		{
			RenderCore::EPixelFormat Format;
			int32_t W, H;
			bool operator<(const UavKey& o) const;
		};

		struct RtKey
		{
			RenderCore::EPixelFormat Format;
			int32_t W, H;
			uint32_t NumMips;
			bool IsMS;
			bool Depth;
			bool operator<(const RtKey& o) const;
		};

		static uint64_t EstimateTextureBytes(RenderCore::EPixelFormat Format, int32_t W, int32_t H, uint32_t NumMips);

		uint64_t FrameCounter = 0;
		uint64_t BudgetBytes = kDefaultBudgetBytes;
		uint64_t EstimatedBytesFree = 0;

		std::map<Tex2DKey, std::vector<PoolEntry<RenderCore::RHITexture2D>>> Tex2DFree;
		std::map<UavKey, std::vector<PoolEntry<RenderCore::RHIUnorderedAccessView>>> UavFree;
		std::map<RtKey, std::vector<PoolEntry<RenderCore::RHIRenderTarget>>> RtFree;
	};
}
