#include "RHI/RDGPassExecuteContext.h"
#include "Render/RDGBuilder.h"
#include "Render/RDGUtils.h"
#include "Render/RenderTexturePool.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIUnorderedAccessView.h"
#include "RHI/RHIRenderPass.h"
#include "core/logger.h"
#include "core/strings.h"
#include "core/commandline.h"
#include <chrono>
#include <map>
#include <set>
#include <sstream>

namespace Engine
{
	namespace
	{
		struct FTransientPooledScope final
		{
			FRDGBuilder* Builder = nullptr;
			bool Active = false;

			FTransientPooledScope(FRDGBuilder* InBuilder, RenderCore::DynamicRHI* RHI)
				: Builder(InBuilder)
				, Active(RHI != nullptr && InBuilder != nullptr && InBuilder->HasTransientPooledUAVSpecs())
			{
				if (!Active)
					return;
				Builder->AcquireTransientPooledUAVs(RHI);
			}

			~FTransientPooledScope()
			{
				if (!Active || !Builder)
					return;
				Builder->ReleaseTransientPooledUAVs();
			}
		};

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

	bool FRDGBuilder::ResolvePassIndex(const std::string& PassName, std::size_t& OutIndex) const
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

	std::size_t FRDGBuilder::CollectSchedulingEdges(std::vector<std::pair<int, int>>& OutEdges) const
	{
		OutEdges.clear();
		std::size_t UnresolvedScheduling = 0;
		const int N = static_cast<int>(Passes.size());
		if (N <= 0)
			return 0;

		// std::map / std::set (not unordered_*): avoids MSVC debug bucket-vector teardown AVs seen when heap is stressed
		// or iterators get invalidated in edge cases; N is tiny (pass count).
		std::set<std::string> ImportNames;
		for (const FRDGPassResource& I : Imports)
		{
			if (ResourceNameForScheduling(I.Name))
				ImportNames.insert(I.Name);
		}

		std::map<std::string, int> LastWriter;

		for (int J = 0; J < N; ++J)
		{
			const FRDGPassDescriptor& Pass = Passes[static_cast<std::size_t>(J)];

			for (const FRDGPassResource& In : Pass.Inputs)
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
							  L"FRDG: pass \"%S\" reads \"%S\" but no prior writer or ImportTexture",
							  Pass.Name.c_str(), In.Name.c_str());
				}
			}

			for (const FRDGPassResource& Out : Pass.Outputs)
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
						  L"FRDG: unresolved scheduling edge \"%S\" -> \"%S\"",
						  Names.first.c_str(), Names.second.c_str());
				continue;
			}
			if (U != V)
				OutEdges.emplace_back(static_cast<int>(U), static_cast<int>(V));
		}

		return UnresolvedScheduling;
	}

	bool FRDGBuilder::BuildExecutionOrderFromEdges(const std::vector<std::pair<int, int>>& Edges, std::vector<std::size_t>& OutOrder) const
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
			core::LOG(core::log_err,
					  L"FRDG: cycle in pass dependencies; graph will not execute (no fallback order). Fix edges or AddPassDependency.");
			OutOrder.clear();
			return false;
		}

		return true;
	}

	void FRDGBuilder::ApplyPassCulling(const std::vector<std::pair<int, int>>& Edges, const std::vector<std::size_t>& FullTopoOrder,
									   const FRDGCompileParameters& Params, std::vector<std::size_t>& OutOrder, FRDGCompileStats& Stats) const
	{
		OutOrder = FullTopoOrder;
		if (!Params.bPassCullingFromSinks)
			return;

		const std::size_t N = Passes.size();
		bool AnySink = false;
		for (const FRDGPassDescriptor& P : Passes)
		{
			if ((P.PassFlags & RDG_GraphSink) != 0)
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
			if ((Passes[I].PassFlags & RDG_GraphSink) != 0)
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
			const FRDGPassDescriptor& P = Passes[Idx];
			const bool MayCull = (P.PassFlags & RDG_MayCullIfUnreachableFromSink) != 0;
			const bool Reach = Visited[Idx] != 0;
			if (MayCull && !Reach)
			{
				Stats.PassCountCulled++;
				continue;
			}
			OutOrder.push_back(Idx);
		}
	}

	void FRDGBuilder::LogNonGraphicsQueueWarnings() const
	{
		for (std::size_t I = 0; I < Passes.size(); ++I)
		{
			const FRDGPassDescriptor& P = Passes[I];
			if (P.Queue != ERDGPassQueue::Graphics)
			{
				const wchar_t* Q = L"?";
				if (P.Queue == ERDGPassQueue::AsyncCompute)
					Q = L"AsyncCompute";
				else if (P.Queue == ERDGPassQueue::Copy)
					Q = L"Copy";
				core::LOG(core::log_war,
						  L"FRDG: pass \"%S\" uses queue %S; only Graphics ordering is implemented - treat as unordered relative to other queues.",
						  P.Name.c_str(), Q);
			}
		}
	}

	void FRDGBuilder::DumpDotToLog(const std::vector<std::pair<int, int>>& Edges) const
	{
		std::ostringstream Dot;
		Dot << "digraph FRDG {\n";
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
		core::LOG(core::log_inf, L"FRDG DOT:\n%S", core::u8_ucs2(S).c_str());
	}

	FRDGBuilder::FRDGBuilder() = default;

	FRDGBuilder::~FRDGBuilder()
	{
		ReleaseTransientPooledUAVs();
	}

	void FRDGBuilder::RegisterTransientUAV(std::string Name, std::function<FRDGTransientUAVDesc()> ResolveDesc)
	{
		FTransientUAVRegistration Reg;
		Reg.Name = std::move(Name);
		Reg.ResolveDesc = std::move(ResolveDesc);
		RegisteredTransientUAVs.emplace_back(std::move(Reg));
	}

	std::shared_ptr<RenderCore::RHIUnorderedAccessView> FRDGBuilder::GetTransientUAV(const std::string& Name) const
	{
		const auto It = LiveTransientUAVByName.find(Name);
		if (It == LiveTransientUAVByName.end())
			return {};
		return It->second;
	}

	void FRDGBuilder::AcquireTransientPooledUAVs(RenderCore::DynamicRHI* RHI)
	{
		ReleaseTransientPooledUAVs();
		TransientAcquireRHI = RHI;
		if (!RHI || RegisteredTransientUAVs.empty())
			return;

		for (const FTransientUAVRegistration& Spec : RegisteredTransientUAVs)
		{
			if (!Spec.ResolveDesc)
				continue;
			FRDGTransientUAVDesc D = Spec.ResolveDesc();
			if (!D.IsAllocatable())
				continue;

			LiveTransientUAVByName[Spec.Name] =
				RHI->RHICreateUnorderedAccessViewForTransientPool(D.PixelFormat, D.Width, D.Height, true);
		}
	}

	void FRDGBuilder::ReleaseTransientPooledUAVs()
	{
		// Frame-scoped aliasing UAVs: destroy at release so slots return to the heap (not RenderTexturePool).
		LiveTransientUAVByName.clear();
		TransientAcquireRHI = nullptr;
	}

	void FRDGBuilder::Clear()
	{
		ReleaseTransientPooledUAVs();
		RegisteredTransientUAVs.clear();
		Imports.clear();
		Passes.clear();
		SchedulingEdges.clear();
		LastCompiledOrder.clear();
	}

	void FRDGBuilder::ImportTexture(std::string Name, std::function<std::shared_ptr<RenderCore::RHITexture2D>()> Resolve, bool Required)
	{
		FRDGPassResource R;
		R.Name = std::move(Name);
		R.Resolve = std::move(Resolve);
		R.Required = Required;
		Imports.emplace_back(std::move(R));
	}

	void FRDGBuilder::AddPass(FRDGPassDescriptor Pass)
	{
		Passes.emplace_back(std::move(Pass));
	}

	void FRDGBuilder::AddPassDependency(std::string ProducerPassName, std::string ConsumerPassName)
	{
		SchedulingEdges.emplace_back(std::move(ProducerPassName), std::move(ConsumerPassName));
	}

	bool FRDGBuilder::Compile(const FRDGCompileParameters& Params, FRDGCompileStats* OutStats)
	{
		FRDGCompileStats LocalStats;
		FRDGCompileStats& Stats = OutStats ? *OutStats : LocalStats;
		Stats = FRDGCompileStats{};
		Stats.PassCountSetup = Passes.size();
		LastCompiledOrder.clear();

		std::vector<std::pair<int, int>> Edges;
		Stats.bUnresolvedSchedulingEdge = CollectSchedulingEdges(Edges) != 0;

		std::vector<std::size_t> FullOrder;
		if (!BuildExecutionOrderFromEdges(Edges, FullOrder))
		{
			Stats.bHadCycle = true;
			if (Params.bLogCompileSummary)
				core::LOG(core::log_err, L"FRDG Compile: failed (cycle); scheduled=0");
			if (Params.bDumpDotToLog)
				DumpDotToLog(Edges);
			return false;
		}

		ApplyPassCulling(Edges, FullOrder, Params, LastCompiledOrder, Stats);
		Stats.PassCountScheduled = LastCompiledOrder.size();

		if (Params.bWarnOnNonGraphicsPassQueues)
			LogNonGraphicsQueueWarnings();

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
					  L"FRDG Compile: setup=%zu scheduled=%zu culled=%zu cycle=%d order=%S",
					  Stats.PassCountSetup,
					  Stats.PassCountScheduled,
					  Stats.PassCountCulled,
					  Stats.bHadCycle ? 1 : 0,
					  core::u8_ucs2(O).c_str());
		}

		return true;
	}

	void FRDGBuilder::ExecutePassesImpl(const FRDGCompileParameters& Params, const std::vector<std::size_t>& Order)
	{
		std::vector<RenderCore::FRDGTextureBarrierDesc> BarrierScratch;
		if (Params.PassCpuTimingsOut)
			Params.PassCpuTimingsOut->clear();

		const bool bWantCpuRows = (Params.PassCpuTimingsOut != nullptr);
		const bool bGpuTimestamps =
			bWantCpuRows && Params.RDGBarrierCommandContext != nullptr && !core::CommandLine::Get().GetName("rdg_no_gpu_timestamps");

		std::map<std::string, double> GpuMsPrev;
		std::vector<std::pair<std::string, double>> GpuConsumeScratch;
		if (bGpuTimestamps)
		{
			Params.RDGBarrierCommandContext->RDGTryConsumePreviousFrameGpuPassTimings(GpuConsumeScratch);
			for (const auto& P : GpuConsumeScratch)
				GpuMsPrev[P.first] = P.second;
			Params.RDGBarrierCommandContext->RDGBeginGpuPassTimingFrame();
		}

		FTransientPooledScope Transients(this, Params.RDGAcquirePooledResourcesRHI);

		for (std::size_t Idx : Order)
		{
			const FRDGPassDescriptor& Pass = Passes[Idx];
			if (!ValidatePass(Pass))
				continue;
			bool bDupSuppress = false;
			if (Params.bRDGAutoPipelineBarriers && Params.RDGBarrierCommandContext)
			{
				if (Pass.bUnbindRenderTargetsBeforeRDGBarriers)
				{
					const RenderCore::FRHIRenderPassDesc UnbindOm{};
					RenderCore::RHIBeginRenderPass(*Params.RDGBarrierCommandContext, UnbindOm);
				}
				BarrierScratch.clear();
				FRDGUtils::AppendPassTextureBarriers(Pass, BarrierScratch);
				if (!BarrierScratch.empty())
				{
					Params.RDGBarrierCommandContext->RDGApplyPassBeginBarriers(BarrierScratch.data(), BarrierScratch.size(), Pass.Queue);
					bDupSuppress = true;
				}
			}
			const RenderCore::FRDGScopedNestedPipelineBarrierDupSuppress DupSuppressScope(bDupSuppress);
			if (Pass.Execute)
			{
				if (Params.PassCpuTimingsOut)
				{
					const auto t0 = std::chrono::high_resolution_clock::now();
					Pass.Execute();
					const auto t1 = std::chrono::high_resolution_clock::now();
					const double msCpu = std::chrono::duration<double, std::milli>(t1 - t0).count();
					double msGpu = -1.0;
					if (bGpuTimestamps)
					{
						const auto It = GpuMsPrev.find(Pass.Name);
						if (It != GpuMsPrev.end())
							msGpu = It->second;
					}
					Params.PassCpuTimingsOut->push_back(FRDGPassCpuTiming{ Pass.Name, msCpu, msGpu });
				}
				else
					Pass.Execute();
				if (bGpuTimestamps)
					Params.RDGBarrierCommandContext->RDGWriteGpuTimestampAfterPass(Pass.Name.c_str());
			}
		}

		if (bGpuTimestamps)
			Params.RDGBarrierCommandContext->RDGResolveGpuPassTimingsEndOfFrame();

		if (Params.bLogRenderTexturePoolStats)
		{
			const RenderTexturePool::Stats S = RenderTexturePool::Get().GetStats();
			core::LOG(core::log_inf,
					  L"RenderTexturePool (post-FRDG ExecutePasses): frame=%llu freeTex2D=%zu freeUav=%zu freeRt=%zu estFreeMB=%.2f budgetMB=%.2f",
					  (unsigned long long)S.FrameCounter,
					  S.FreeTex2D,
					  S.FreeUav,
					  S.FreeRt,
					  S.EstimatedBytesFree / (1024.0 * 1024.0),
					  S.BudgetBytes / (1024.0 * 1024.0));
		}
	}

	void FRDGBuilder::ExecutePasses(const FRDGCompileParameters& Params)
	{
		if (LastCompiledOrder.empty())
		{
			if (!Passes.empty())
				core::LOG(core::log_err, L"FRDG ExecutePasses: no valid schedule (call Compile first, or compile failed). Skipping passes.");
			return;
		}

		ExecutePassesImpl(Params, LastCompiledOrder);
	}

	void FRDGBuilder::ExecutePassesInSetupOrder(const FRDGCompileParameters& Params)
	{
		std::vector<std::size_t> Order;
		Order.reserve(Passes.size());
		for (std::size_t i = 0; i < Passes.size(); ++i)
			Order.push_back(i);
		ExecutePassesImpl(Params, Order);
	}

	bool FRDGBuilder::CompileAndExecute(const FRDGCompileParameters& Params)
	{
		if (!Compile(Params, nullptr))
		{
			core::LOG(core::log_err, L"FRDG CompileAndExecute: compile failed; running passes in AddPass order.");
			ExecutePassesInSetupOrder(Params);
			return false;
		}
		ExecutePasses(Params);
		return true;
	}

	bool FRDGBuilder::ValidatePass(const FRDGPassDescriptor& Pass) const
	{
		if (!Pass.Execute)
		{
			core::LOG(core::log_war, L"FRDG pass has no execute callback: %S", Pass.Name.c_str());
			return false;
		}

		for (const FRDGPassResource& Input : Pass.Inputs)
		{
			if (Input.Required && (!Input.Resolve || !Input.Resolve()))
			{
				core::LOG(core::log_war, L"FRDG pass missing input: %S.%S", Pass.Name.c_str(), Input.Name.c_str());
				return false;
			}
		}

		if (Pass.ValidateOutputs)
		{
			for (const FRDGPassResource& Output : Pass.Outputs)
			{
				if (Output.Required && (!Output.Resolve || !Output.Resolve()))
				{
					core::LOG(core::log_war, L"FRDG pass missing output: %S.%S", Pass.Name.c_str(), Output.Name.c_str());
					return false;
				}
			}
		}

		return true;
	}

} // namespace Engine
