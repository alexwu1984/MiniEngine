#pragma once
#include "core/inc.h"
#include <cstddef>
#include <vector>

namespace RenderCore
{
	class RHITexture2D;
}

namespace Engine
{
	// Slot used for pass inputs/outputs and for imported (persistent) textures.
	struct FrameGraphResource
	{
		std::string Name;
		std::function<std::shared_ptr<RenderCore::RHITexture2D>()> Resolve;
		bool Required = true;
	};

	struct FramePassDesc
	{
		std::string Name;
		std::vector<FrameGraphResource> Inputs;
		std::vector<FrameGraphResource> Outputs;
		std::function<void()> Execute;
		bool ValidateOutputs = false;
	};

	// Phase 1: ordered pass list + ImportTexture registry.
	// Phase 2: edges from resource names (last writer before reader), Kahn topo sort;
	//          ties broken by original pass index to preserve declaration order when unconstrained.
	class FrameGraph
	{
	public:
		void Clear();

		// Available before the first pass (no producer in this graph), e.g. external history for SSR.
		void ImportTexture(std::string Name, std::function<std::shared_ptr<RenderCore::RHITexture2D>()> Resolve, bool Required = true);

		void AddPass(FramePassDesc Pass);
		void Execute();

	private:
		bool ValidatePass(const FramePassDesc& Pass) const;
		bool BuildExecutionOrder(std::vector<std::size_t>& OutOrder) const;

		std::vector<FrameGraphResource> Imports;
		std::vector<FramePassDesc> Passes;
	};
}
