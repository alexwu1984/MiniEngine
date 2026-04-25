#include "Render/PostProcessGraph.h"

namespace Engine
{
	void PostProcessGraph::AddPass(RenderPassDesc Pass)
	{
		Passes.emplace_back(std::move(Pass));
	}

	void PostProcessGraph::Execute()
	{
		for (const RenderPassDesc& Pass : Passes)
		{
			if (Pass.Execute)
				Pass.Execute();
		}
	}
}
