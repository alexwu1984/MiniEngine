#include "Render/FrameGraph.h"
#include "core/logger.h"
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace Engine
{
	namespace
	{
		bool ResourceNameForScheduling(const std::string& Name)
		{
			return !Name.empty();
		}
	}

	bool FrameGraph::BuildExecutionOrder(std::vector<std::size_t>& OutOrder) const
	{
		OutOrder.clear();
		const std::size_t N = Passes.size();
		if (N == 0)
			return true;

		std::unordered_set<std::string> ImportNames;
		ImportNames.reserve(Imports.size() * 2);
		for (const FrameGraphResource& I : Imports)
		{
			if (ResourceNameForScheduling(I.Name))
				ImportNames.insert(I.Name);
		}

		std::unordered_map<std::string, int> LastWriter;
		std::vector<std::vector<std::size_t>> Adj(N);
		std::vector<int> Indegree(N, 0);

		for (std::size_t J = 0; J < N; ++J)
		{
			const FramePassDesc& Pass = Passes[J];

			for (const FrameGraphResource& In : Pass.Inputs)
			{
				if (!ResourceNameForScheduling(In.Name))
					continue;

				const auto ItW = LastWriter.find(In.Name);
				if (ItW != LastWriter.end() && ItW->second >= 0)
				{
					const int W = ItW->second;
					if (W != static_cast<int>(J))
					{
						Adj[static_cast<std::size_t>(W)].push_back(J);
						Indegree[J]++;
					}
				}
				else if (!ImportNames.count(In.Name) && In.Required)
				{
					core::LOG(core::log_war,
							  L"FrameGraph: pass \"%S\" reads \"%S\" but no prior writer or ImportTexture",
							  Pass.Name.c_str(), In.Name.c_str());
				}
			}

			for (const FrameGraphResource& Out : Pass.Outputs)
			{
				if (ResourceNameForScheduling(Out.Name))
					LastWriter[Out.Name] = static_cast<int>(J);
			}
		}

		std::set<std::size_t> Ready;
		for (std::size_t I = 0; I < N; ++I)
		{
			if (Indegree[I] == 0)
				Ready.insert(I);
		}

		while (!Ready.empty())
		{
			const std::size_t U = *Ready.begin();
			Ready.erase(Ready.begin());
			OutOrder.push_back(U);
			for (std::size_t V : Adj[U])
			{
				Indegree[V]--;
				if (Indegree[V] == 0)
					Ready.insert(V);
			}
		}

		if (OutOrder.size() != N)
		{
			core::LOG(core::log_err, L"FrameGraph: cycle in pass dependencies; falling back to AddPass declaration order");
			OutOrder.resize(N);
			for (std::size_t I = 0; I < N; ++I)
				OutOrder[I] = I;
			return false;
		}

		return true;
	}

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
		std::vector<std::size_t> Order;
		BuildExecutionOrder(Order);

		for (std::size_t Idx : Order)
		{
			const FramePassDesc& Pass = Passes[Idx];
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
