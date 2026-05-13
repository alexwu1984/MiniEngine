#pragma once

/**
 * Wall-clock diagnostics (std::chrono::steady_clock). For startup / CPU-side splits; not GPU timestamps.
 *
 * Log schema (ASCII literals in code; file saved UTF-8 recommended):
 *   Perf|<domain>|<event> key=value ...
 * grep examples: Perf|  or  Perf|boot|
 *
 * Emit control: core::inf() + perf::hdr(...) lines are skipped unless the process is started with -perfinf
 * (core::perf::ShouldEmitPerfInfLogs).
 *
 * Domains (stable):
 *   boot       Application shell: WinMain path, viewer Init.
 *   engine     MainEngine::Init, worker threads, optional notes.
 *   render_rt  Render-thread resource setup (e.g. WorldSceneRender::InitResource lambda).
 *   shader_jit First-hit RHICreate*Shader CPU stalls. Events include DeferredLightingInit (fallback tex + CBs only),
 *              DeferredLightingJitShaders (DeferredLighting VS/PS on first ExecuteRaster), PBRMaterialInit, PBRTranslucentForwardJit (lazy forward PS).
 *              Lines omitted when wall_ms < 10 unless CLI -perfshaderjitverbose (cache-hit spam).
 *   scene      Scene reload / world swap (ReloadSceneJson phases).
 *   frame      First-frame rollups (e.g. RDG pass CPU sum).
 *   tick       Game-thread Tick when over threshold (viewport / submit / sync splits).
 *   render_rec Render recording thread frame boundary (ExecuteFrame wall vs submission_seq).
 */

#include <chrono>
#include <cstring>
#include <string>

namespace core
{
	class WallSplitTimer
	{
	public:
		WallSplitTimer() noexcept
			: t0_(std::chrono::steady_clock::now())
			, mark_(t0_)
		{
		}

		double split_ms() noexcept
		{
			const auto n = std::chrono::steady_clock::now();
			const double ms = std::chrono::duration<double, std::milli>(n - mark_).count();
			mark_ = n;
			return ms;
		}

		double total_ms() const noexcept
		{
			return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0_).count();
		}

		std::chrono::steady_clock::time_point origin() const noexcept { return t0_; }

	private:
		std::chrono::steady_clock::time_point t0_;
		std::chrono::steady_clock::time_point mark_;
	};

	namespace perf
	{
		inline std::string hdr(const char* domain, const char* event)
		{
			const size_t dl = std::strlen(domain);
			const size_t el = std::strlen(event);
			std::string s;
			s.reserve(6u + dl + el);
			s.append("Perf|", 5u);
			s.append(domain, dl);
			s.push_back('|');
			s.append(event, el);
			s.push_back(' ');
			return s;
		}

		constexpr const char kBoot[] = "boot";
		constexpr const char kEngine[] = "engine";
		constexpr const char kRenderRt[] = "render_rt";
		constexpr const char kShaderJit[] = "shader_jit";
		constexpr const char kScene[] = "scene";
		constexpr const char kFrame[] = "frame";
		constexpr const char kTick[] = "tick";
		constexpr const char kRenderRec[] = "render_rec";

		/** Off by default. Pass -perfinf to enable core::inf() lines that use perf::hdr (Perf|domain|event ...). */
		bool ShouldEmitPerfInfLogs();
	} // namespace perf
} // namespace core
