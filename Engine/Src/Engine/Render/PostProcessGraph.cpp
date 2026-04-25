#include "Render/PostProcessGraph.h"
#include "core/logger.h"

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
			if (!ValidatePass(Pass))
				continue;
			Pass.Execute();
		}
	}

	bool PostProcessGraph::ValidatePass(const RenderPassDesc& Pass) const
	{
		if (!Pass.Execute)
		{
			core::LOG(core::log_war, L"PostProcessGraph pass has no execute callback: %S", Pass.Name.c_str());
			return false;
		}

		for (const RenderPassResource& Input : Pass.Inputs)
		{
			if (Input.Required && (!Input.Resolve || !Input.Resolve()))
			{
				core::LOG(core::log_war, L"PostProcessGraph pass missing input: %S.%S", Pass.Name.c_str(), Input.Name.c_str());
				return false;
			}
		}

		if (Pass.ValidateOutputs)
		{
			for (const RenderPassResource& Output : Pass.Outputs)
			{
				if (Output.Required && (!Output.Resolve || !Output.Resolve()))
				{
					core::LOG(core::log_war, L"PostProcessGraph pass missing output: %S.%S", Pass.Name.c_str(), Output.Name.c_str());
					return false;
				}
			}
		}

		return true;
	}
}
