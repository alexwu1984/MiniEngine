#pragma once
#include "core/inc.h"
#include "RHI/RHIDefinitions.h"
#include <map>
#include <memory>
#include <vector>

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
	class RenderTexturePool
	{
	public:
		static RenderTexturePool& Get();

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

		// Optional hooks: FrameGraph compile/execute boundary for future accounting or per-frame trim.
		void BeginFrame();
		void EndFrame();

	private:
		RenderTexturePool() = default;

		static constexpr std::size_t kMaxFreePerKey = 8;

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

		std::map<Tex2DKey, std::vector<std::shared_ptr<RenderCore::RHITexture2D>>> Tex2DFree;
		std::map<UavKey, std::vector<std::shared_ptr<RenderCore::RHIUnorderedAccessView>>> UavFree;
		std::map<RtKey, std::vector<std::shared_ptr<RenderCore::RHIRenderTarget>>> RtFree;
	};
}
