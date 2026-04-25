#include "Render/FrameGraph.h"
#include "core/logger.h"

namespace Engine
{
	void FrameGraph::Clear()
	{
		Imports.clear();
		Passes.clear();
	}

	void FrameGraph::ImportTexture(std::string Name, std::function<std::shared_ptr<RenderCore::RHITexture2D>()> Resolve, bool Required)
	{
		FrameGraphResource R;
		R.Name = std::move(Name);
		R.Resolve = std::move(Resolve);
		R.Required = Required;
		Imports.emplace_back(std::move(R));
	}

	void FrameGraph::AddPass(FramePassDesc Pass)
	{
		Passes.emplace_back(std::move(Pass));
	}

	void FrameGraph::Execute()
	{
		for (const FramePassDesc& Pass : Passes)
		{
			if (!ValidatePass(Pass))
				continue;
			Pass.Execute();
		}
	}

	bool FrameGraph::ValidatePass(const FramePassDesc& Pass) const
	{
		if (!Pass.Execute)
		{
			core::LOG(core::log_war, L"FrameGraph pass has no execute callback: %S", Pass.Name.c_str());
			return false;
		}

		for (const FrameGraphResource& Input : Pass.Inputs)
		{
			if (Input.Required && (!Input.Resolve || !Input.Resolve()))
			{
				core::LOG(core::log_war, L"FrameGraph pass missing input: %S.%S", Pass.Name.c_str(), Input.Name.c_str());
				return false;
			}
		}

		if (Pass.ValidateOutputs)
		{
			for (const FrameGraphResource& Output : Pass.Outputs)
			{
				if (Output.Required && (!Output.Resolve || !Output.Resolve()))
				{
					core::LOG(core::log_war, L"FrameGraph pass missing output: %S.%S", Pass.Name.c_str(), Output.Name.c_str());
					return false;
				}
			}
		}

		return true;
	}
}
