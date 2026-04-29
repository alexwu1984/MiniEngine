#include "Render/FrameGraph.h"
#include "core/logger.h"
#include "core/strings.h"
#include <sstream>
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

		std::string DotEscapeLabel(const std::string& S)
		{
			std::string R;
			R.reserve(S.size() + 8);
			for (char C : S)
			{
				if (C == '"' || C == '\\')
					R.push_back('\\');
				R.push_back(C);
			}
			return R;
		}
	} // namespace

	bool FrameGraph::ResolvePassIndex(const std::string& PassName, std::size_t& OutIndex) const
	{
		for (std::size_t I = 0; I < Passes.size(); ++I)
		{
			if (Passes[I].Name == PassName)
			{
				OutIndex = I;
				return true;
			}
		}
		return false;
	}

	std::size_t FrameGraph::CollectSchedulingEdges(std::vector<std::pair<int, int>>& OutEdges) const
	{
		OutEdges.clear();
		std::size_t UnresolvedScheduling = 0;
		const int N = static_cast<int>(Passes.size());
		if (N <= 0)
			return 0;

		std::unordered_set<std::string> ImportNames;
		ImportNames.reserve(Imports.size() * 2);
		for (const FrameGraphResource& I : Imports)
		{
			if (ResourceNameForScheduling(I.Name))
				ImportNames.insert(I.Name);
		}

		std::unordered_map<std::string, int> LastWriter;

		for (int J = 0; J < N; ++J)
		{
			const FramePassDesc& Pass = Passes[static_cast<std::size_t>(J)];

			for (const FrameGraphResource& In : Pass.Inputs)
			{
				if (!ResourceNameForScheduling(In.Name))
					continue;

				const auto ItW = LastWriter.find(In.Name);
				if (ItW != LastWriter.end() && ItW->second >= 0)
				{
					const int W = ItW->second;
					if (W != J)
						OutEdges.emplace_back(W, J);
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
					LastWriter[Out.Name] = J;
			}
		}

		for (const auto& Names : SchedulingEdges)
		{
			std::size_t U = 0, V = 0;
			if (!ResolvePassIndex(Names.first, U) || !ResolvePassIndex(Names.second, V))
			{
				++UnresolvedScheduling;
				core::LOG(core::log_war,
						  L"FrameGraph: unresolved scheduling edge \"%S\" -> \"%S\"",
						  Names.first.c_str(), Names.second.c_str());
				continue;
			}
			if (U != V)
				OutEdges.emplace_back(static_cast<int>(U), static_cast<int>(V));
		}

		return UnresolvedScheduling;
	}

	bool FrameGraph::BuildExecutionOrderFromEdges(const std::vector<std::pair<int, int>>& Edges, std::vector<std::size_t>& OutOrder) const
	{
		OutOrder.clear();
		const std::size_t N = Passes.size();
		if (N == 0)
			return true;

		std::vector<std::vector<std::size_t>> Adj(N);
		std::vector<int> Indegree(N, 0);

		std::set<std::pair<int, int>> Unique;
		for (const auto& E : Edges)
		{
			if (E.first < 0 || E.second < 0 || E.first == E.second)
				continue;
			const std::size_t U = static_cast<std::size_t>(E.first);
			const std::size_t V = static_cast<std::size_t>(E.second);
			if (U >= N || V >= N)
				continue;
			Unique.insert(E);
		}
		for (const auto& E : Unique)
		{
			const std::size_t U = static_cast<std::size_t>(E.first);
			const std::size_t V = static_cast<std::size_t>(E.second);
			Adj[U].push_back(V);
			Indegree[V]++;
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

	void FrameGraph::ApplyPassCulling(const std::vector<std::pair<int, int>>& Edges, const std::vector<std::size_t>& FullTopoOrder,
									const FrameGraphCompileParams& Params, std::vector<std::size_t>& OutOrder, RDGCompileStats& Stats) const
	{
		OutOrder = FullTopoOrder;
		if (!Params.bPassCullingFromSinks)
			return;

		const std::size_t N = Passes.size();
		bool AnySink = false;
		for (const FramePassDesc& P : Passes)
		{
			if ((P.PassFlags & ERGPass_GraphSink) != 0)
			{
				AnySink = true;
				break;
			}
		}
		if (!AnySink)
			return;

		std::vector<std::vector<int>> Rev(N);
		for (const auto& E : Edges)
		{
			if (E.first < 0 || E.second < 0)
				continue;
			const std::size_t U = static_cast<std::size_t>(E.first);
			const std::size_t V = static_cast<std::size_t>(E.second);
			if (U < N && V < N && U != V)
				Rev[V].push_back(static_cast<int>(U));
		}

		std::vector<uint8_t> Visited(N, 0);
		std::vector<int> Stack;
		for (std::size_t I = 0; I < N; ++I)
		{
			if ((Passes[I].PassFlags & ERGPass_GraphSink) != 0)
				Stack.push_back(static_cast<int>(I));
		}

		while (!Stack.empty())
		{
			const int V = Stack.back();
			Stack.pop_back();
			if (V < 0 || static_cast<std::size_t>(V) >= N)
				continue;
			if (Visited[static_cast<std::size_t>(V)])
				continue;
			Visited[static_cast<std::size_t>(V)] = 1;
			for (int U : Rev[static_cast<std::size_t>(V)])
			{
				if (U >= 0 && static_cast<std::size_t>(U) < N && !Visited[static_cast<std::size_t>(U)])
					Stack.push_back(U);
			}
		}

		OutOrder.clear();
		for (std::size_t Idx : FullTopoOrder)
		{
			const FramePassDesc& P = Passes[Idx];
			const bool MayCull = (P.PassFlags & ERGPass_MayCullIfUnreachableFromSink) != 0;
			const bool Reach = Visited[Idx] != 0;
			if (MayCull && !Reach)
			{
				Stats.PassCountCulled++;
				continue;
			}
			OutOrder.push_back(Idx);
		}
	}

	void FrameGraph::DumpDotToLog(const std::vector<std::pair<int, int>>& Edges) const
	{
		std::ostringstream Dot;
		Dot << "digraph FrameGraph {\n";
		for (std::size_t I = 0; I < Passes.size(); ++I)
		{
			Dot << "  p" << I << " [label=\"" << DotEscapeLabel(Passes[I].Name) << "\"];\n";
		}
		for (const auto& E : Edges)
		{
			if (E.first >= 0 && E.second >= 0)
				Dot << "  p" << E.first << " -> p" << E.second << ";\n";
		}
		Dot << "}\n";
		const std::string S = Dot.str();
		core::LOG(core::log_inf, L"FrameGraph DOT:\n%S", core::u8_ucs2(S).c_str());
	}

	void FrameGraph::Clear()
	{
		Imports.clear();
		Passes.clear();
		SchedulingEdges.clear();
		LastCompiledOrder.clear();
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
		// Graph sink: swap-chain submission (legacy name "Present" or split pipeline "RHISubmitAndPresent").
		if (Pass.Name == "Present" || Pass.Name == "RHISubmitAndPresent")
		{
			Pass.PassFlags |= ERGPass_GraphSink;
			if (!Passes.empty())
				SchedulingEdges.emplace_back(Passes.back().Name, Pass.Name);
		}
		Passes.emplace_back(std::move(Pass));
	}

	void FrameGraph::AddPassDependency(std::string ProducerPassName, std::string ConsumerPassName)
	{
		SchedulingEdges.emplace_back(std::move(ProducerPassName), std::move(ConsumerPassName));
	}

	bool FrameGraph::Compile(const FrameGraphCompileParams& Params, RDGCompileStats* OutStats)
	{
		RDGCompileStats LocalStats;
		RDGCompileStats& Stats = OutStats ? *OutStats : LocalStats;
		Stats = RDGCompileStats{};
		Stats.PassCountSetup = Passes.size();

		std::vector<std::pair<int, int>> Edges;
		Stats.bUnresolvedSchedulingEdge = CollectSchedulingEdges(Edges) != 0;

		std::vector<std::size_t> FullOrder;
		Stats.bHadCycle = !BuildExecutionOrderFromEdges(Edges, FullOrder);

		ApplyPassCulling(Edges, FullOrder, Params, LastCompiledOrder, Stats);
		Stats.PassCountScheduled = LastCompiledOrder.size();

		if (Params.bDumpDotToLog)
			DumpDotToLog(Edges);

		if (Params.bLogCompileSummary)
		{
			std::ostringstream OrderText;
			for (std::size_t K = 0; K < LastCompiledOrder.size(); ++K)
			{
				if (K)
					OrderText << " -> ";
				OrderText << Passes[LastCompiledOrder[K]].Name;
			}
			const std::string O = OrderText.str();
			core::LOG(core::log_inf,
					  L"FrameGraph Compile: setup=%zu scheduled=%zu culled=%zu cycle=%d order=%S",
					  Stats.PassCountSetup,
					  Stats.PassCountScheduled,
					  Stats.PassCountCulled,
					  Stats.bHadCycle ? 1 : 0,
					  core::u8_ucs2(O).c_str());
		}

		return !Stats.bHadCycle;
	}

	void FrameGraph::Execute(const FrameGraphCompileParams& Params)
	{
		Compile(Params, nullptr);

		for (std::size_t Idx : LastCompiledOrder)
		{
			const FramePassDesc& Pass = Passes[Idx];
			if (!ValidatePass(Pass))
				continue;
			if (Pass.Execute)
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

} // namespace Engine
