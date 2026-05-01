#pragma once
#include "core/inc.h"
#include "RHI/RDGResourceAccess.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace RenderCore
{
	class RHITexture2D;
	class RHICommandContext;
}

namespace Engine
{
using FRDGResourceAccess = RenderCore::FRDGResourceAccess;
using ERDGPassQueue = RenderCore::ERDGPassQueue;

enum ERDGPassFlags : uint32_t
{
	RDG_None = 0,
	RDG_Raster = 1u << 0,
	RDG_Compute = 1u << 1,
	RDG_Copy = 1u << 2,
	/** Culled when compile enables sink reachability culling and pass is not an ancestor of any GraphSink. */
	RDG_MayCullIfUnreachableFromSink = 1u << 3,
	/** Reachability root for culling (typically swap chain; pass name Present or RHISubmitAndPresent). */
	RDG_GraphSink = 1u << 4,
};

/** Placeholder for transient texture allocation (future pool). */
struct FRDGTextureDesc
{
	uint32_t Width = 0;
	uint32_t Height = 0;
	uint8_t MipCount = 1;
	uint8_t MSAA_Count = 1;
};

/** One named texture slot on a pass (resolve + access hint). */
struct FRDGPassResource
{
	std::string Name;
	std::function<std::shared_ptr<RenderCore::RHITexture2D>()> Resolve;
	bool Required = true;
	FRDGResourceAccess Access = FRDGResourceAccess::Unknown;
	/** 0xFFFFFFFF = whole resource; per-mip/slice later. */
	uint32_t SubresourceIndex = 0xFFFFFFFFu;
};

/** Pass definition: declared inputs/outputs, execute lambda, flags and queue. */
struct FRDGPassDescriptor
{
	std::string Name;
	std::vector<FRDGPassResource> Inputs;
	std::vector<FRDGPassResource> Outputs;
	std::function<void()> Execute;
	bool ValidateOutputs = false;
	uint32_t PassFlags = RDG_Raster;
	ERDGPassQueue Queue = ERDGPassQueue::Graphics;
	/**
	 * When applying RDG barriers, OM bindings can make transitions illegal (e.g. RTV-bound color -> COPY_SOURCE,
	 * DSV-bound depth -> SRV). If true, clears RTV/DSV binds on the barrier command context before transitions.
	 */
	bool bUnbindRenderTargetsBeforeRDGBarriers = false;
};

struct FRDGCompileStats
{
	std::size_t PassCountSetup = 0;
	std::size_t PassCountScheduled = 0;
	std::size_t PassCountCulled = 0;
	bool bHadCycle = false;
	bool bUnresolvedSchedulingEdge = false;
};

struct FRDGCompileParameters
{
	bool bPassCullingFromSinks = true;
	bool bDumpDotToLog = false;
	bool bLogCompileSummary = false;
	/** After ExecutePasses(): log RenderTexturePool::GetStats() (compile/execute RDG boundary hook). */
	bool bLogRenderTexturePoolStats = false;
	/** Log when a pass uses AsyncCompute/Copy: multi-queue execution ordering is not implemented. */
	bool bWarnOnNonGraphicsPassQueues = true;
	/**
	 * When set with bRDGAutoPipelineBarriers, ExecutePasses inserts pass-begin transitions from FRDGResourceAccess
	 * (UE RDG-style pipeline barriers). Must point at the same command list passes record into.
	 */
	RenderCore::RHICommandContext* RDGBarrierCommandContext = nullptr;
	/** If false, skips RDGApplyPassBeginBarriers even when RDGBarrierCommandContext is set. */
	bool bRDGAutoPipelineBarriers = true;
};

/**
 * Frame render graph: ImportTexture / AddPass / AddPassDependency,
 * Compile (ordering, optional sink reachability culling, optional DOT dump),
 * ExecutePasses (runs the last successful Compile order; does not call Compile).
 *
 * Scheduling edges come from (1) resource name flow: each output name remembers the last pass that
 * wrote it; a pass that lists that name as an input depends on that writer, and (2) AddPassDependency
 * for producer/consumer pairs without a shared RDG texture name (e.g. shadow maps).
 * Multi-step producers split across passes when helpful (e.g. deferred lighting: copy SceneColor ->
 * SceneColorPreLighting, then raster writes lit SceneColor).
 * FRDGResourceAccess drives pass-begin resource transitions when FRDGCompileParameters supplies an RHI command context
 * and bRDGAutoPipelineBarriers (UE-style Epilogue): Unknown skips a slot; declare RTV/SRV/DSV/Copy* as appropriate.
 */
class FRDGBuilder
{
public:
	void Clear();

	void ImportTexture(std::string Name, std::function<std::shared_ptr<RenderCore::RHITexture2D>()> Resolve, bool Required = true);

	void AddPass(FRDGPassDescriptor Pass);

	/** Scheduling edge only (no shared texture): Producer must finish before Consumer. */
	void AddPassDependency(std::string ProducerPassName, std::string ConsumerPassName);

	bool Compile(const FRDGCompileParameters& Params = {}, FRDGCompileStats* OutStats = nullptr);

	/** Run passes in LastCompiledOrder. Call Compile() first each frame; on compile failure this is a no-op. */
	void ExecutePasses(const FRDGCompileParameters& Params = {});

	/** Run passes in AddPass order (recovery when Compile() fails due to a cycle). Still validates inputs per pass. */
	void ExecutePassesInSetupOrder(const FRDGCompileParameters& Params = {});

	/** Compile() then ExecutePasses(); on compile failure runs ExecutePassesInSetupOrder and returns false. */
	bool CompileAndExecute(const FRDGCompileParameters& Params = {});

private:
	bool ValidatePass(const FRDGPassDescriptor& Pass) const;
	std::size_t CollectSchedulingEdges(std::vector<std::pair<int, int>>& OutEdges) const;
	bool BuildExecutionOrderFromEdges(const std::vector<std::pair<int, int>>& Edges, std::vector<std::size_t>& OutOrder) const;
	void ApplyPassCulling(const std::vector<std::pair<int, int>>& Edges, const std::vector<std::size_t>& FullTopoOrder,
						  const FRDGCompileParameters& Params, std::vector<std::size_t>& OutOrder, FRDGCompileStats& Stats) const;
	bool ResolvePassIndex(const std::string& PassName, std::size_t& OutIndex) const;
	void DumpDotToLog(const std::vector<std::pair<int, int>>& Edges) const;
	void LogNonGraphicsQueueWarnings() const;
	void ExecutePassesImpl(const FRDGCompileParameters& Params, const std::vector<std::size_t>& Order);

	std::vector<FRDGPassResource> Imports;
	std::vector<FRDGPassDescriptor> Passes;
	std::vector<std::pair<std::string, std::string>> SchedulingEdges;
	std::vector<std::size_t> LastCompiledOrder;
};

} // namespace Engine
