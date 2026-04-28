#pragma once
#include "D3D12/D3D12RHICommon.h"
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12Allocation.h"
#include "D3D12/D3D12CallStats.h"
#include "win/com_ptr.h"

namespace RenderCore
{
	class D3D12CommandAllocator 
	{
	public:
		explicit D3D12CommandAllocator(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType);
		~D3D12CommandAllocator();

		// The command allocator is ready to be reset when all command lists have been executed (or discarded) AND the GPU not using it.
		inline bool IsReady() const { return (PendingCommandListCount.load() == 0) && SyncPoint.IsComplete(); }
		inline bool HasValidSyncPoint() const { return SyncPoint.IsValid(); }
		inline void SetSyncPoint(const D3D12SyncPoint& InSyncPoint) { Assert(InSyncPoint.IsValid()); SyncPoint = InSyncPoint; }
		inline void Reset() { Assert(IsReady()); CommandAllocator->Reset(); }

		operator ID3D12CommandAllocator* () { return CommandAllocator.get(); }

		// Called to indicate a command list is using this command alloctor
		inline void IncrementPendingCommandLists()
		{
			Assert(PendingCommandListCount.load() >= 0);
			++PendingCommandListCount;
		}

		// Called to indicate a command list using this allocator has been executed OR discarded (closed with no intention to execute it).
		inline void DecrementPendingCommandLists()
		{
			// During shutdown or error paths a command list can be discarded/released without having
			// incremented this counter (e.g. if reset failed or teardown raced). Don't crash the app
			// on accounting mismatches; clamp at 0 and continue.
			int32_t v = PendingCommandListCount.load();
			if (v <= 0)
				return;
			--PendingCommandListCount;
		}

	private:
		void Init(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType);

	private:
		win32::com_ptr<ID3D12CommandAllocator> CommandAllocator;
		D3D12SyncPoint SyncPoint;	// Indicates when the GPU is finished using the command allocator.
		// The number of command lists using this allocator but haven't been executed yet.
		// Must start at 0; leaving atomics default-initialized is undefined and can corrupt allocator readiness logic.
		std::atomic_int32_t PendingCommandListCount{ 0 };
	};

	class FD3D12Device;
	class FD3D12CommandListManager;
	class D3D12CommandContext;
	class D3D12UniformBuffer;

	class D3D12CommandListHandle
	{
	private:
		typedef std::pair<uint64_t, D3D12SyncPoint>	GenerationSyncPointPair;	// Pair of command list generation to a sync point

		class D3D12CommandListData : public FD3D12DeviceChild, public D3D12RefCount
		{
		public:
			D3D12CommandListData(std::weak_ptr<FD3D12Device> ParentDevice, D3D12_COMMAND_LIST_TYPE InCommandListType, D3D12CommandAllocator& CommandAllocator, FD3D12CommandListManager* InCommandListManager);
			~D3D12CommandListData();

			void Close();
			// Reset the command list with a specified command allocator and optional initial state.
			// Note: Command lists can be reset immediately after they are submitted for execution.
			void Reset(D3D12CommandAllocator& CommandAllocator);

			bool IsComplete(uint64_t Generation)
			{
				if (Generation >= CurrentGeneration)
				{
					// Have not submitted this generation for execution yet.
					return false;
				}

				Assert(Generation < CurrentGeneration);
				if (Generation > LastCompleteGeneration)
				{
					std::lock_guard<std::recursive_mutex> Lock(ActiveGenerationsCS);
					GenerationSyncPointPair GenerationSyncPoint;
					if (!ActiveGenerations.empty())
					{
						GenerationSyncPoint = ActiveGenerations.front();
						if (Generation < GenerationSyncPoint.first)
						{
							// The requested generation is older than the oldest tracked generation, so it must be complete.
							return true;
						}
						else
						{
							if (GenerationSyncPoint.second.IsComplete())
							{
								// Oldest tracked generation is done so clean the queue and try again.
								CleanupActiveGenerations();
								return IsComplete(Generation);
							}
							else
							{
								// The requested generation is newer than the older track generation but the old one isn't done.
								return false;
							}
						}
					}
				}

				return true;
			}

			void WaitForCompletion(uint64_t Generation)
			{
				if (Generation > LastCompleteGeneration)
				{
					CleanupActiveGenerations();
					if (Generation > LastCompleteGeneration)
					{
						std::unique_lock<std::recursive_mutex> Lock(ActiveGenerationsCS);
						//ensureMsgf(Generation < CurrentGeneration, TEXT("You can't wait for an unsubmitted command list to complete.  Kick first!"));
						GenerationSyncPointPair GenerationSyncPoint;
						while (!ActiveGenerations.empty() && (Generation > LastCompleteGeneration))
						{
							GenerationSyncPoint = ActiveGenerations.front();
							Assert(Generation >= GenerationSyncPoint.first);
							ActiveGenerations.pop();

							// Unblock other threads while we wait for the command list to complete
							ActiveGenerationsCS.unlock();

							GenerationSyncPoint.second.WaitForCompletion();

							ActiveGenerationsCS.lock();
							LastCompleteGeneration = (std::max)(LastCompleteGeneration, GenerationSyncPoint.first);
						}
					}
				}
			}

			inline void CleanupActiveGenerations()
			{
				std::unique_lock<std::recursive_mutex> Lock(ActiveGenerationsCS);

				// Cleanup the queue of active command list generations.
				// Only remove them from the queue when the GPU has completed them.
				GenerationSyncPointPair GenerationSyncPoint;
				while (!ActiveGenerations.empty())
				{

					GenerationSyncPoint = ActiveGenerations.front();
					if (!GenerationSyncPoint.second.IsComplete())
					{
						break;
					}
					// The GPU is done with the work associated with this generation, remove it from the queue.
					ActiveGenerations.pop();

					Assert(GenerationSyncPoint.first > LastCompleteGeneration);
					LastCompleteGeneration = GenerationSyncPoint.first;
				}
			}

			void SetSyncPoint(const D3D12SyncPoint& SyncPoint)
			{
				{
					std::unique_lock<std::recursive_mutex> Lock(ActiveGenerationsCS);

					// Only valid sync points should be set otherwise we might not wait on the GPU correctly.
					Assert(SyncPoint.IsValid());

					// Track when this command list generation is completed on the GPU.
					GenerationSyncPointPair CurrentGenerationSyncPoint;
					CurrentGenerationSyncPoint.first = CurrentGeneration;
					CurrentGenerationSyncPoint.second = SyncPoint;
					ActiveGenerations.push(CurrentGenerationSyncPoint);

					// Move to the next generation of the command list.
					CurrentGeneration++;
				}

				// Update the associated command allocator's sync point so it's not reset until the GPU is done with all command lists using it.
				CurrentCommandAllocator->SetSyncPoint(SyncPoint);
			}

			void FlushResourceBarriers()
			{
				const uint32_t n = (uint32_t)ResourceBarrierBatcher.GetBarriers().size();
				// Avoid spamming empty flushes (they add CPU overhead and can amplify driver-side work).
				if (n == 0)
					return;
				Render::D3D12CallStats::FlushResourceBarriers(n);
				ResourceBarrierBatcher.Flush(CommandList.get());
			}


			FD3D12CommandListManager* CommandListManager;
			D3D12CommandContext* CurrentOwningContext;
			const D3D12_COMMAND_LIST_TYPE			CommandListType;
			win32::com_ptr<ID3D12GraphicsCommandList>	CommandList;		// Raw D3D command list pointer
			win32::com_ptr<ID3D12GraphicsCommandList1> CommandList1;
#if D3D12_RHI_RAYTRACING
			win32::com_ptr<ID3D12GraphicsCommandList4> RayTracingCommandList;
#endif // D3D12_RHI_RAYTRACING
			// Array of resources who's state needs to be synced between submits.
			std::vector<FD3D12PendingResourceBarrier>	PendingResourceBarriers;

			/**
			*	A map of all D3D resources, and their states, that were state transitioned with tracking.
			*/
			class FCommandListResourceState
			{
			private:
				std::map<FD3D12Resource*, CResourceState> ResourceStates;
				void inline ConditionalInitalize(FD3D12Resource* pResource, CResourceState& ResourceState);

			public:
				CResourceState& GetResourceState(FD3D12Resource* pResource);

				// After ExecuteCommandLists, copy per-list predicted states into FD3D12Resource global tracking
				// so the next submit sees correct Before values (immediate barriers must not advance global during record).
				void CommitTrackedStatesToGlobal();

				// Empty the command list's resource state map after the command list is executed
				void Empty();

				// Drop per-list tracking for a resource that is about to be destroyed (e.g. ImGui buffer resize).
				void RemoveResourceState(FD3D12Resource* pResource);
			};

			FCommandListResourceState TrackedResourceState;

			D3D12CommandAllocator*	CurrentCommandAllocator;	// Command allocator currently being used for recording the command list
			uint64_t				CurrentGeneration;
			uint64_t				LastCompleteGeneration;
			// Increments on every Reset(). D3D12 command list pointer is stable across Reset(),
			// but all bindings are cleared; caches must treat each Reset() as a fresh recording session.
			uint64_t				RecordingGeneration;
			bool					IsClosed;
			bool					bShouldTrackStartEndTime;
			std::queue<GenerationSyncPointPair>			ActiveGenerations;	// Queue of active command list generations and their sync points. Used to determine what command lists have been completed on the GPU.
			std::recursive_mutex						ActiveGenerationsCS;	// While only a single thread can record to a command list at any given time, multiple threads can ask for the state of a given command list. So the associated tracking must be thread-safe.
			// Batches resource barriers together until it's explicitly flushed
			FD3D12ResourceBarrierBatcher ResourceBarrierBatcher;


			FD3D12LinearAllocator UploadLinearAllocator;
			FD3D12LinearAllocator DefaultLinearAllocator;

			std::vector<D3D12UniformBuffer*> PendingUniformBuffersFenceTag;
			void AddUniformBufferFenceTag(D3D12UniformBuffer* ub);
			void FlushPendingUniformBufferFenceTags(uint64_t fenceValue);
			void CancelPendingUniformBufferFenceTags();
		};
	public:
		D3D12CommandListHandle() : CommandListData(nullptr) {}

		D3D12CommandListHandle(const D3D12CommandListHandle& CL)
			: D3D12CommandListHandle(CL.CommandListData)
		{}

		D3D12CommandListHandle(D3D12CommandListData* InData)
			: CommandListData(InData)
		{
			if (CommandListData)
			{
				CommandListData->AddRef();
			}
		}

		D3D12CommandListHandle(D3D12CommandListHandle&& CL)
			: CommandListData(CL.CommandListData)
		{
			CL.CommandListData = nullptr;
		}

		virtual ~D3D12CommandListHandle()
		{
			if (CommandListData)
				CommandListData->Release();
		}

		D3D12CommandListHandle& operator = (const D3D12CommandListHandle& CL)
		{
			if (this != &CL)
			{
				if (CommandListData)
					CommandListData->Release();

				CommandListData = nullptr;

				if (CL.CommandListData)
				{
					CommandListData = CL.CommandListData;
					CommandListData->AddRef();
				}
			}

			return *this;
		}

		D3D12CommandListHandle& operator=(D3D12CommandListHandle&& CL)
		{
			if (CommandListData != CL.CommandListData)
			{
				if (CommandListData)
					CommandListData->Release();
				CommandListData = CL.CommandListData;
				CL.CommandListData = nullptr;
			}
			return *this;
		}

		bool operator!() const
		{
			return CommandListData == 0;
		}

		inline friend bool operator==(const D3D12CommandListHandle& lhs, const D3D12CommandListHandle& rhs)
		{
			return lhs.CommandListData == rhs.CommandListData;
		}

		inline friend bool operator==(const D3D12CommandListHandle& lhs, const D3D12CommandListData* rhs)
		{
			return lhs.CommandListData == rhs;
		}

		inline friend bool operator==(const D3D12CommandListData* lhs, const D3D12CommandListHandle& rhs)
		{
			return lhs == rhs.CommandListData;
		}

		inline friend bool operator!=(const D3D12CommandListHandle& lhs, const D3D12CommandListData* rhs)
		{
			return lhs.CommandListData != rhs;
		}

		inline friend bool operator!=(const D3D12CommandListData* lhs, const D3D12CommandListHandle& rhs)
		{
			return lhs != rhs.CommandListData;
		}

		FORCEINLINE ID3D12GraphicsCommandList* operator->() const
		{
			Assert(CommandListData && !CommandListData->IsClosed);
			return CommandListData->CommandList.get();
		}

		void Create(std::weak_ptr<FD3D12Device> ParentDevice, D3D12_COMMAND_LIST_TYPE CommandListType, D3D12CommandAllocator& CommandAllocator, FD3D12CommandListManager* InCommandListManager);

		uint64_t ExecuteAndClear(bool WaitForCompletion = false);
		void Execute(bool WaitForCompletion = false);

		void RegisterUniformBufferForSubmitFence(D3D12UniformBuffer* ub) const;
		void FlushPendingUniformBufferFenceTags(uint64_t fenceValue);
		void CancelPendingUniformBufferFenceTags();
		ED3D12CommandQueueType GetSubmitFenceQueueType() const;

		/** Ring uniform: Record + Set* in one call. Copy lists assert; graphics APIs require DIRECT; compute APIs allow DIRECT or COMPUTE. */
		void SetGraphicsRootConstantBufferViewUniform(UINT RootParameterIndex, D3D12UniformBuffer* UniformBuffer) const;
		void SetComputeRootConstantBufferViewUniform(UINT RootParameterIndex, D3D12UniformBuffer* UniformBuffer) const;
		void SetGraphicsRoot32BitConstantsFromUniform(UINT RootParameterIndex, UINT Num32BitValues, D3D12UniformBuffer* UniformBuffer, UINT DestOffsetIn32BitValues = 0) const;
		void SetComputeRoot32BitConstantsFromUniform(UINT RootParameterIndex, UINT Num32BitValues, D3D12UniformBuffer* UniformBuffer, UINT DestOffsetIn32BitValues = 0) const;

		// Fence-tied recycling for linear allocators + dynamic descriptor heaps (same work as ExecuteAndClear's post-submit hook).
		// Required for command lists submitted via ExecuteAndIncrementFence (e.g. async-compute resource-barrier batches on the direct queue).
		void CleanupTransientResources(uint64_t FenceValue, ED3D12CommandQueueType QueueType);

		void Close()
		{
			Assert(CommandListData);
			CommandListData->Close();
		}

		// Reset the command list with a specified command allocator and optional initial state.
		// Note: Command lists can be reset immediately after they are submitted for execution.
		void Reset(D3D12CommandAllocator& CommandAllocator)
		{
			Assert(CommandListData);
			CommandListData->Reset(CommandAllocator);
		}

		ID3D12CommandList* CommandList() const
		{
			Assert(CommandListData);
			return CommandListData->CommandList.get();
		}

		ID3D12GraphicsCommandList* GraphicsCommandList() const
		{
			Assert(CommandListData && (CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT || CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE));
			return reinterpret_cast<ID3D12GraphicsCommandList*>(CommandListData->CommandList.get());
		}

		ID3D12GraphicsCommandList1* GraphicsCommandList1() const
		{
			Assert(CommandListData && (CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT || CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE));
			return CommandListData->CommandList1.get();
		}

#if D3D12_RHI_RAYTRACING
		ID3D12GraphicsCommandList4* RayTracingCommandList() const
		{
			check(CommandListData && (CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT || CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE));
			return CommandListData->RayTracingCommandList.GetReference();
		}
#endif // D3D12_RHI_RAYTRACING
		uint64_t CurrentGeneration() const
		{
			Assert(CommandListData);
			return CommandListData->CurrentGeneration;
		}

		uint64_t GetRecordingGeneration() const
		{
			Assert(CommandListData);
			return CommandListData->RecordingGeneration;
		}


		D3D12CommandAllocator* CurrentCommandAllocator()
		{
			Assert(CommandListData);
			return CommandListData->CurrentCommandAllocator;
		}

		void SetSyncPoint(const D3D12SyncPoint& SyncPoint)
		{
			Assert(CommandListData);
			CommandListData->SetSyncPoint(SyncPoint);
		}

		bool IsClosed() const
		{
			Assert(CommandListData);
			return CommandListData->IsClosed;
		}

		bool IsComplete(uint64_t Generation) const
		{
			if (CommandListData) // Can be null with mGPU
			{
				return CommandListData->IsComplete(Generation);
			}
			else
			{
				return true;
			}
		}

		void WaitForCompletion(uint64_t Generation) const
		{
			Assert(CommandListData);
			return CommandListData->WaitForCompletion(Generation);
		}

		// Get the state of a resource on this command lists.
// This is only used for resources that require state tracking.
		CResourceState& GetResourceState(FD3D12Resource* pResource)
		{
			Assert(CommandListData);
			return CommandListData->TrackedResourceState.GetResourceState(pResource);
		}

		void AddPendingResourceBarrier(FD3D12Resource* Resource, D3D12_RESOURCE_STATES State, uint32_t SubResource)
		{
			Assert(CommandListData);

			FD3D12PendingResourceBarrier PRB = { Resource, State, SubResource };
			CommandListData->PendingResourceBarriers.push_back(PRB);
		}

		std::vector<FD3D12PendingResourceBarrier>& PendingResourceBarriers()
		{
			Assert(CommandListData);
			return CommandListData->PendingResourceBarriers;
		}

		// Empty all the resource states being tracked on this command list
		void EmptyTrackedResourceState()
		{
			Assert(CommandListData);
			CommandListData->TrackedResourceState.Empty();
		}

		void CommitTrackedResourceStateToGlobal()
		{
			Assert(CommandListData);
			CommandListData->TrackedResourceState.CommitTrackedStatesToGlobal();
		}

		void RemoveTrackedResourceState(FD3D12Resource* pResource)
		{
			Assert(CommandListData);
			CommandListData->TrackedResourceState.RemoveResourceState(pResource);
		}

		void SetCurrentOwningContext(D3D12CommandContext* context)
		{
			CommandListData->CurrentOwningContext = context;
		}

		D3D12CommandContext* GetCurrentOwningContext()
		{
			return CommandListData->CurrentOwningContext;
		}

		D3D12_COMMAND_LIST_TYPE GetCommandListType() const
		{
			Assert(CommandListData);
			return CommandListData->CommandListType;
		}

		// Adds a transition barrier to the barrier batch
		void AddTransitionBarrier(FD3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32_t Subresource);

		// Adds a UAV barrier to the barrier batch
		void AddUAVBarrier();

		void AddAliasingBarrier(FD3D12Resource* pResource);

		// Flushes the batched resource barriers to the current command list
		void FlushResourceBarriers()
		{
			Assert(CommandListData);
			CommandListData->FlushResourceBarriers();
		}

		FD3D12LinearAllocator& GetLinearAllocator(EFastAllocatorType type)
		{
			Assert(CommandListData);
			if (type == UploadFastAllocator)
			{
				return CommandListData->UploadLinearAllocator;
			}
			else
			{
				return CommandListData->DefaultLinearAllocator;
			}
		}

	private:
		D3D12CommandListHandle& operator*()
		{
			return *this;
		}

		D3D12CommandListData* CommandListData = nullptr;
	};

	class D3D12CLSyncPoint
	{
	public:
		D3D12CLSyncPoint() : Generation(0) {}
		D3D12CLSyncPoint(D3D12CommandListHandle& CL) : CommandList(CL), Generation(CL.CommandList() ? CL.CurrentGeneration() : 0) {}
		D3D12CLSyncPoint(D3D12CommandListHandle&& CL) : CommandList(std::move(CL)), Generation(CommandList.CommandList() ? CommandList.CurrentGeneration() : 0) {}
		D3D12CLSyncPoint(const D3D12CLSyncPoint& SyncPoint) : CommandList(SyncPoint.CommandList), Generation(SyncPoint.Generation) {}

		D3D12CLSyncPoint& operator = (D3D12CommandListHandle& CL)
		{
			CommandList = CL;
			Generation = (CL != nullptr) ? CL.CurrentGeneration() : 0;

			return *this;
		}

		D3D12CLSyncPoint& operator=(D3D12CommandListHandle&& CL)
		{
			Generation = (CL != nullptr) ? CL.CurrentGeneration() : 0;
			CommandList = std::move(CL);
			return *this;
		}

		D3D12CLSyncPoint& operator = (const D3D12CLSyncPoint& SyncPoint)
		{
			CommandList = SyncPoint.CommandList;
			Generation = SyncPoint.Generation;

			return *this;
		}

		bool operator!() const
		{
			return CommandList == 0;
		}

		bool IsValid() const
		{
			return CommandList != nullptr;
		}

		bool IsOpen() const
		{
			return Generation == CommandList.CurrentGeneration();
		}

		bool IsComplete() const
		{
			return CommandList.IsComplete(Generation);
		}

		void WaitForCompletion() const
		{
			CommandList.WaitForCompletion(Generation);
		}

		uint64_t GetGeneration() const
		{
			return Generation;
		}

	private:

		friend class FD3D12CommandListManager;

		D3D12CommandListHandle CommandList;
		uint64_t                  Generation;
	};
}