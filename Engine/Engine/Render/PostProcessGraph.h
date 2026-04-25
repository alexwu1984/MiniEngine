#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHITexture2D;
}

namespace Engine
{
	struct RenderPassDesc
	{
		std::string Name;
		std::vector<std::shared_ptr<RenderCore::RHITexture2D>> Inputs;
		std::vector<std::shared_ptr<RenderCore::RHITexture2D>> Outputs;
		std::function<void()> Execute;
	};

	class PostProcessGraph
	{
	public:
		void AddPass(RenderPassDesc Pass);
		void Execute();

	private:
		std::vector<RenderPassDesc> Passes;
	};
}
