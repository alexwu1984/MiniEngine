#pragma once
#include "core/inc.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace RenderCore
{
	class RHITexture2D;
}

namespace Engine
{
/** Shader / RHI view access (compile-time metadata for future barrier batching). */
enum class ERGTextureAccess : uint8_t
{
	Unknown = 0,
	SRV,
	UAV,
	RTV,
	DSV,
	CopySrc,
	CopyDst,
};

enum ERGPassFlags : uint32_t
{
	ERGPass_None = 0,
	ERGPass_Raster = 1u << 0,
	ERGPass_Compute = 1u << 1,
	ERGPass_Copy = 1u << 2,
	/** Culled when compile enables sink reachability culling and pass is not an ancestor of any GraphSink. */
	ERGPass_MayCullIfUnreachableFromSink = 1u << 3,
	/** Reachability root for culling (typically swap chain; pass name Present or RHISubmitAndPresent). */
	ERGPass_GraphSink = 1u << 4,
};

enum class ERGQueueType : uint8_t
{
	Graphics = 0,
	AsyncCompute = 1,
	Copy = 2,
};

/** Placeholder for transient texture allocation (future pool). */
struct RDGTextureDesc
{
	uint32_t Width = 0;
	uint32_t Height = 0;
	uint8_t MipCount = 1;
	uint8_t MSAA_Count = 1;
};

struct FrameGraphResource
{
	std::string Name;
	std::function<std::shared_ptr<RenderCore::RHITexture2D>()> Resolve;
	bool Required = true;
	ERGTextureAccess Access = ERGTextureAccess::Unknown;
	/** 0xFFFFFFFF = whole resource; per-mip/slice later. */
	uint32_t SubresourceIndex = 0xFFFFFFFFu;
};

struct FramePassDesc
{
	std::string Name;
	std::vector<FrameGraphResource> Inputs;
	std::vector<FrameGraphResource> Outputs;
	std::function<void()> Execute;
	bool ValidateOutputs = false;
	/** Appended after legacy {Name, IO, Execute} initializer lists (flags / queue). */
	uint32_t PassFlags = ERGPass_Raster;
	ERGQueueType Queue = ERGQueueType::Graphics;
};

struct RDGCompileStats
{
	std::size_t PassCountSetup = 0;
	std::size_t PassCountScheduled = 0;
	std::size_t PassCountCulled = 0;
	bool bHadCycle = false;
	bool bUnresolvedSchedulingEdge = false;
};

struct FrameGraphCompileParams
{
	bool bPassCullingFromSinks = false;
	bool bDumpDotToLog = false;
	bool bLogCompileSummary = false;
};

/**
 * Render graph: Setup (ImportTexture / AddPass / AddPassDependency),
 * Compile (ordering, optional sink reachability culling, debug dump),
 * Execute (last Compile() order only).
 */
class FrameGraph
{
public:
	void Clear();

	void ImportTexture(std::string Name, std::function<std::shared_ptr<RenderCore::RHITexture2D>()> Resolve, bool Required = true);

	void AddPass(FramePassDesc Pass);

	/** Scheduling edge only (no shared texture): Producer must finish before Consumer. */
	void AddPassDependency(std::string ProducerPassName, std::string ConsumerPassName);

	bool Compile(const FrameGraphCompileParams& Params = {}, RDGCompileStats* OutStats = nullptr);

	void Execute(const FrameGraphCompileParams& Params = {});

private:
	bool ValidatePass(const FramePassDesc& Pass) const;
	/** Returns count of user scheduling edges that failed name resolution. */
	std::size_t CollectSchedulingEdges(std::vector<std::pair<int, int>>& OutEdges) const;
	bool BuildExecutionOrderFromEdges(const std::vector<std::pair<int, int>>& Edges, std::vector<std::size_t>& OutOrder) const;
	void ApplyPassCulling(const std::vector<std::pair<int, int>>& Edges, const std::vector<std::size_t>& FullTopoOrder,
						  const FrameGraphCompileParams& Params, std::vector<std::size_t>& OutOrder, RDGCompileStats& Stats) const;
	bool ResolvePassIndex(const std::string& PassName, std::size_t& OutIndex) const;
	void DumpDotToLog(const std::vector<std::pair<int, int>>& Edges) const;

	std::vector<FrameGraphResource> Imports;
	std::vector<FramePassDesc> Passes;
	std::vector<std::pair<std::string, std::string>> SchedulingEdges;
	std::vector<std::size_t> LastCompiledOrder;
};

} // namespace Engine
