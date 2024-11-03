#pragma once
#include "D3D12/D3D12RHICommon.h"
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "D3D12/D3D12Resource.h"
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
		inline void SetSyncPoint(const D3D12SyncPoint& InSyncPoint) { assert(InSyncPoint.IsValid()); SyncPoint = InSyncPoint; }
		inline void Reset() { assert(IsReady()); CommandAllocator->Reset(); }

		operator ID3D12CommandAllocator* () { return CommandAllocator.get(); }

		// Called to indicate a command list is using this command alloctor
		inline void IncrementPendingCommandLists()
		{
			assert(PendingCommandListCount.load() >= 0);
			++PendingCommandListCount;
		}

		// Called to indicate a command list using this allocator has been executed OR discarded (closed with no intention to execute it).
		inline void DecrementPendingCommandLists()
		{
			assert(PendingCommandListCount.load() > 0);
			--PendingCommandListCount;
		}

	private:
		void Init(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType);

	private:
		win32::com_ptr<ID3D12CommandAllocator> CommandAllocator;
		D3D12SyncPoint SyncPoint;	// Indicates when the GPU is finished using the command allocator.
		std::atomic_int32_t PendingCommandListCount;	// The number of command lists using this allocator but haven't been executed yet.
	};

	class FD3D12Device;
	class FD3D12CommandListManager;
	class D3D12CommandContext;

	class D3D12CommandListHandle
	{
	private:
		typedef std::pair<uint64_t, D3D12SyncPoint>	GenerationSyncPointPair;	// Pair of command list generation to a sync point

		class D3D12CommandListData : public FD3D12DeviceChild
		{
		public:
			D3D12CommandListData(FD3D12Device* ParentDevice, D3D12_COMMAND_LIST_TYPE InCommandListType, D3D12CommandAllocator& CommandAllocator, FD3D12CommandListManager* InCommandListManager);
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

				assert(Generation < CurrentGeneration);
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
							assert(Generation >= GenerationSyncPoint.first);
							ActiveGenerations.pop();

							// Unblock other threads while we wait for the command list to complete
							ActiveGenerationsCS.unlock();

							GenerationSyncPoint.second.WaitForCompletion();

							ActiveGenerationsCS.lock();
							LastCompleteGeneration = std::max(LastCompleteGeneration, GenerationSyncPoint.first);
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

					assert(GenerationSyncPoint.first > LastCompleteGeneration);
					LastCompleteGeneration = GenerationSyncPoint.first;
				}
			}

			void SetSyncPoint(const D3D12SyncPoint& SyncPoint)
			{
				{
					std::unique_lock<std::recursive_mutex> Lock(ActiveGenerationsCS);

					// Only valid sync points should be set otherwise we might not wait on the GPU correctly.
					assert(SyncPoint.IsValid());

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
				ResourceBarrierBatcher.Flush(CommandList.get());
			}

			uint32_t AddRef() const
			{
				int32_t NewValue = ++NumRefs;
				assert(NewValue > 0);
				return uint32_t(NewValue);
			}

			uint32_t Release() const
			{
				int32_t NewValue = --NumRefs;
				assert(NewValue >= 0);
				return uint32_t(NewValue);
			}

			mutable std::atomic_int32_t	NumRefs;
			FD3D12CommandListManager* CommandListManager;
			D3D12CommandContext* CurrentOwningContext;
			const D3D12_COMMAND_LIST_TYPE			CommandListType;
			win32::com_ptr<ID3D12GraphicsCommandList>	CommandList;		// Raw D3D command list pointer
			win32::com_ptr<ID3D12GraphicsCommandList1> CommandList1;
#if D3D12_RHI_RAYTRACING
			win32::com_ptr<ID3D12GraphicsCommandList4> RayTracingCommandList;
#endif // D3D12_RHI_RAYTRACING
			// Array of resources who's state needs to be synced between submits.
			std::vector<D3D12PendingResourceBarrier>	PendingResourceBarriers;

			/**
			*	A map of all D3D resources, and their states, that were state transitioned with tracking.
			*/
			class FCommandListResourceState
			{
			private:
				std::map<D3D12Resource*, CResourceState> ResourceStates;
				void inline ConditionalInitalize(D3D12Resource* pResource, CResourceState& ResourceState);

			public:
				CResourceState& GetResourceState(D3D12Resource* pResource);

				// Empty the command list's resource state map after the command list is executed
				void Empty();
			};

			FCommandListResourceState TrackedResourceState;

			D3D12CommandAllocator*	CurrentCommandAllocator;	// Command allocator currently being used for recording the command list
			uint64_t				CurrentGeneration;
			uint64_t				LastCompleteGeneration;
			bool					IsClosed;
			bool					bShouldTrackStartEndTime;
			std::queue<GenerationSyncPointPair>			ActiveGenerations;	// Queue of active command list generations and their sync points. Used to determine what command lists have been completed on the GPU.
			std::recursive_mutex						ActiveGenerationsCS;	// While only a single thread can record to a command list at any given time, multiple threads can ask for the state of a given command list. So the associated tracking must be thread-safe.
			// Batches resource barriers together until it's explicitly flushed
			FD3D12ResourceBarrierBatcher ResourceBarrierBatcher;
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
			if (CommandListData && CommandListData->Release() == 0)
			{
				delete CommandListData;
			}
		}

		D3D12CommandListHandle& operator = (const D3D12CommandListHandle& CL)
		{
			if (this != &CL)
			{
				if (CommandListData && CommandListData->Release() == 0)
				{
					delete CommandListData;
				}

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
				if (CommandListData && CommandListData->Release() == 0)
				{
					delete CommandListData;
				}
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
			assert(CommandListData && !CommandListData->IsClosed);

			return CommandListData->CommandList.get();
		}

		void Create(FD3D12Device* ParentDevice, D3D12_COMMAND_LIST_TYPE CommandListType, D3D12CommandAllocator& CommandAllocator, FD3D12CommandListManager* InCommandListManager);

		void Execute(bool WaitForCompletion = false);

		void Close()
		{
			assert(CommandListData);
			CommandListData->Close();
		}

		// Reset the command list with a specified command allocator and optional initial state.
		// Note: Command lists can be reset immediately after they are submitted for execution.
		void Reset(D3D12CommandAllocator& CommandAllocator)
		{
			assert(CommandListData);
			CommandListData->Reset(CommandAllocator);
		}

		ID3D12CommandList* CommandList() const
		{
			assert(CommandListData);
			return CommandListData->CommandList.get();
		}

		ID3D12GraphicsCommandList* GraphicsCommandList() const
		{
			assert(CommandListData && (CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT || CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE));
			return reinterpret_cast<ID3D12GraphicsCommandList*>(CommandListData->CommandList.get());
		}

		ID3D12GraphicsCommandList1* GraphicsCommandList1() const
		{
			assert(CommandListData && (CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT || CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE));
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
			assert(CommandListData);
			return CommandListData->CurrentGeneration;
		}

		D3D12CommandAllocator* CurrentCommandAllocator()
		{
			assert(CommandListData);
			return CommandListData->CurrentCommandAllocator;
		}

		void SetSyncPoint(const D3D12SyncPoint& SyncPoint)
		{
			assert(CommandListData);
			CommandListData->SetSyncPoint(SyncPoint);
		}

		bool IsClosed() const
		{
			assert(CommandListData);
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
			assert(CommandListData);
			return CommandListData->WaitForCompletion(Generation);
		}

		// Get the state of a resource on this command lists.
// This is only used for resources that require state tracking.
		CResourceState& GetResourceState(D3D12Resource* pResource)
		{
			assert(CommandListData);
			return CommandListData->TrackedResourceState.GetResourceState(pResource);
		}

		void AddPendingResourceBarrier(D3D12Resource* Resource, D3D12_RESOURCE_STATES State, uint32_t SubResource)
		{
			assert(CommandListData);

			D3D12PendingResourceBarrier PRB = { Resource, State, SubResource };
			CommandListData->PendingResourceBarriers.push_back(PRB);
		}

		std::vector<D3D12PendingResourceBarrier>& PendingResourceBarriers()
		{
			assert(CommandListData);
			return CommandListData->PendingResourceBarriers;
		}

		// Empty all the resource states being tracked on this command list
		void EmptyTrackedResourceState()
		{
			assert(CommandListData);
			CommandListData->TrackedResourceState.Empty();
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
			assert(CommandListData);
			return CommandListData->CommandListType;
		}

		// Adds a transition barrier to the barrier batch
		void AddTransitionBarrier(D3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32_t Subresource);

		// Adds a UAV barrier to the barrier batch
		void AddUAVBarrier();

		void AddAliasingBarrier(D3D12Resource* pResource);

		// Flushes the batched resource barriers to the current command list
		void FlushResourceBarriers()
		{
			assert(CommandListData);
			CommandListData->FlushResourceBarriers();
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