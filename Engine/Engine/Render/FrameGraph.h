#pragma once
#include "core/inc.h"

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

	// Phase 1: ordered pass list + optional import registry for future compile/validation.
	// Execution order matches AddPass order; no automatic scheduling yet.
	class FrameGraph
	{
	public:
		void Clear();

		// Records a persistent texture (e.g. GBuffer, swapchain). Not executed; used for documentation and later dependency checks.
		void ImportTexture(std::string Name, std::function<std::shared_ptr<RenderCore::RHITexture2D>()> Resolve, bool Required = true);

		void AddPass(FramePassDesc Pass);
		void Execute();

	private:
		bool ValidatePass(const FramePassDesc& Pass) const;

		std::vector<FrameGraphResource> Imports;
		std::vector<FramePassDesc> Passes;
	};
}
