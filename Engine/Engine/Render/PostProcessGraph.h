#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHITexture2D;
}

namespace Engine
{
	struct RenderPassResource
	{
		std::string Name;
		std::function<std::shared_ptr<RenderCore::RHITexture2D>()> Resolve;
		bool Required = true;
	};

	struct RenderPassDesc
	{
		std::string Name;
		std::vector<RenderPassResource> Inputs;
		std::vector<RenderPassResource> Outputs;
		std::function<void()> Execute;
		bool ValidateOutputs = false;
	};

	class PostProcessGraph
	{
	public:
		void AddPass(RenderPassDesc Pass);
		void Execute();

	private:
		bool ValidatePass(const RenderPassDesc& Pass) const;
		std::vector<RenderPassDesc> Passes;
	};
}
