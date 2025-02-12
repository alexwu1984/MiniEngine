#pragma once
#include "D3D12/D3D12RHICommon.h"
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "win/com_ptr.h"

namespace RenderCore
{
	class FD3D12Resource;
	class D3D12PendingResourceBarrier
	{
	public:
		FD3D12Resource*			Resource;
		D3D12_RESOURCE_STATES	State;
		uint32_t                SubResource;
	};

	class D3D12RefCount
	{
	public:
		D3D12RefCount()
		{
			NumRefs = 0;
		}
		virtual ~D3D12RefCount()
		{
			assert(NumRefs.load() == 0);
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
			if (NewValue == 0)
			{
				delete this;
				return 0;
			}
			assert(NewValue >= 0);
			return uint32_t(NewValue);
		}
		uint32_t GetRefCount() const
		{
			int32_t CurrentValue = NumRefs.load();
			assert(CurrentValue >= 0);
			return uint32_t(CurrentValue);
		}
	private:
		mutable std::atomic_int32_t NumRefs;
	};

	class FD3D12Resource : public D3D12RefCount,public FD3D12DeviceChild
	{
	private:
		win32::com_ptr<ID3D12Resource> Resource;
		win32::com_ptr<D3D12MA::Allocation> Allocation;
		D3D12_RESOURCE_DESC Desc;
		uint8_t PlaneCount;
		uint16_t SubresourceCount;
		CResourceState ResourceState;
		D3D12_RESOURCE_STATES DefaultResourceState;
		D3D12_RESOURCE_STATES ReadableState;
		D3D12_RESOURCE_STATES WritableState;

		bool bRequiresResourceStateTracking;
		bool bDepthStencil;
		bool bDeferDelete;
		D3D12_HEAP_TYPE HeapType;
		D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddress;
		void* ResourceBaseAddress;
		std::wstring DebugName;

	public:
		explicit FD3D12Resource(std::weak_ptr<FD3D12Device> ParentDevice,
			D3D12MA::Allocation* Allocation,
			ID3D12Resource* InResource,
			D3D12_RESOURCE_STATES InitialState,
			D3D12_RESOURCE_DESC const& InDesc,
			D3D12_HEAP_TYPE InHeapType = D3D12_HEAP_TYPE_DEFAULT);

		virtual ~FD3D12Resource();

		operator ID3D12Resource& () { return *Resource.get(); }
		ID3D12Resource* GetResource() const { return Resource.get(); }

		inline void* Map(const D3D12_RANGE* ReadRange = nullptr)
		{
			Assert(Resource);
			VERIFYD3DRESULT(Resource->Map(0, ReadRange, &ResourceBaseAddress));

			return ResourceBaseAddress;
		}

		inline void Unmap()
		{
			Assert(Resource);
			Assert(ResourceBaseAddress);
			Resource->Unmap(0, nullptr);

			ResourceBaseAddress = nullptr;
		}

		D3D12_RESOURCE_DESC const& GetDesc() const { return Desc; }
		D3D12_HEAP_TYPE GetHeapType() const { return HeapType; }
		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return GPUVirtualAddress; }
		void* GetResourceBaseAddress() const { Assert(ResourceBaseAddress); return ResourceBaseAddress; }
		uint16_t GetMipLevels() const { return Desc.MipLevels; }
		uint16_t GetArraySize() const { return (Desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) ? 1 : Desc.DepthOrArraySize; }
		uint8_t GetPlaneCount() const { return PlaneCount; }
		uint16_t GetSubresourceCount() const { return SubresourceCount; }
		CResourceState& GetResourceState()
		{
			Assert(bRequiresResourceStateTracking);
			// This state is used as the resource's "global" state between command lists. It's only needed for resources that
			// require state tracking.
			return ResourceState;
		}
		D3D12_RESOURCE_STATES GetDefaultResourceState() const { Assert(!bRequiresResourceStateTracking); return DefaultResourceState; }
		D3D12_RESOURCE_STATES GetWritableState() const { return WritableState; }
		D3D12_RESOURCE_STATES GetReadableState() const { return ReadableState; }
		bool RequiresResourceStateTracking() const { return bRequiresResourceStateTracking; }

		void SetName(const wchar_t* Name)
		{
			DebugName = Name;
			if (Resource)
				Resource->SetName(Name);
		}

		std::wstring GetName() const
		{
			return DebugName;
		}

		void DoNotDeferDelete()
		{
			bDeferDelete = false;
		}

		inline bool ShouldDeferDelete() const { return bDeferDelete; }
		void DeferDelete();

		inline bool IsPlacedResource() const { return false; }
		inline bool IsDepthStencilResource() const { return bDepthStencil; }

		struct FD3D12ResourceTypeHelper
		{
			FD3D12ResourceTypeHelper(D3D12_RESOURCE_DESC& Desc, D3D12_HEAP_TYPE HeapType) :
				bSRV((Desc.Flags& D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0),
				bDSV((Desc.Flags& D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0),
				bRTV((Desc.Flags& D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0),
				bUAV((Desc.Flags& D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0),
				bWritable(bDSV || bRTV || bUAV),
				bSRVOnly(bSRV && !bWritable),
				bBuffer(Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER),
				bReadBackResource(HeapType == D3D12_HEAP_TYPE_READBACK)
			{}

			const D3D12_RESOURCE_STATES GetOptimalInitialState(bool bAccurateWriteableStates) const
			{
				if (bSRVOnly)
				{
					return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				}
				else if (bBuffer && !bUAV)
				{
					return (bReadBackResource) ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_GENERIC_READ;
				}
				else if (bWritable)
				{
					if (bAccurateWriteableStates)
					{
						if (bDSV)
						{
							return D3D12_RESOURCE_STATE_DEPTH_WRITE;
						}
						else if (bRTV)
						{
							return D3D12_RESOURCE_STATE_RENDER_TARGET;
						}
						else if (bUAV)
						{
							return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
						}
					}
					else
					{
						// This things require tracking anyway
						return D3D12_RESOURCE_STATE_COMMON;
					}
				}

				return D3D12_RESOURCE_STATE_COMMON;
			}

			const uint32_t bSRV : 1;
			const uint32_t bDSV : 1;
			const uint32_t bRTV : 1;
			const uint32_t bUAV : 1;
			const uint32_t bWritable : 1;
			const uint32_t bSRVOnly : 1;
			const uint32_t bBuffer : 1;
			const uint32_t bReadBackResource : 1;
		};
		private:
			void InitalizeResourceState(D3D12_RESOURCE_STATES InitialState)
			{
				SubresourceCount = GetMipLevels() * GetArraySize() * GetPlaneCount();

#if D3D12_RHI_RAYTRACING
				if (InitialState == D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
				{
					// Ray-tracing acceleration structure resources can never be transitioned out of their initial state.
					bRequiresResourceStateTracking = false;
					WritableState = InitialState;
					ReadableState = InitialState;
				}
				else
#endif // D3D12_RHI_RAYTRACING
				{
					//DetermineResourceStates();
				}

				if (bRequiresResourceStateTracking)
				{
					// Only a few resources (~1%) actually need resource state tracking
					ResourceState.Initialize(SubresourceCount);
					ResourceState.SetResourceState(InitialState);
				}
			}

			void DetermineResourceStates()
			{
				const FD3D12ResourceTypeHelper Type(Desc, HeapType);

				bDepthStencil = Type.bDSV;

				if (Type.bWritable)
				{
					// Determine the resource's write/read states.
					if (Type.bRTV)
					{
						// Note: The resource could also be used as a UAV however we don't store that writable state. UAV's are handled in a separate RHITransitionResources() specially for UAVs so we know the writeable state in that case should be UAV.
						Assert(!Type.bDSV && !Type.bBuffer);
						WritableState = D3D12_RESOURCE_STATE_RENDER_TARGET;
						ReadableState = Type.bSRV ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_CORRUPT;
					}
					else if (Type.bDSV)
					{
						Assert(!Type.bRTV && !Type.bUAV && !Type.bBuffer);
						WritableState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
						ReadableState = Type.bSRV ? D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_DEPTH_READ;
					}
					else
					{
						Assert(Type.bUAV && !Type.bRTV && !Type.bDSV);
						WritableState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
						ReadableState = Type.bSRV ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_CORRUPT;
					}
				}

				if (Type.bBuffer)
				{
					if (!Type.bWritable)
					{
						// Buffer used for input, like Vertex/Index buffer.
						// Don't bother tracking state for this resource.
						DefaultResourceState = (HeapType == D3D12_HEAP_TYPE_READBACK) ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_GENERIC_READ;
						bRequiresResourceStateTracking = false;
						return;
					}
				}
				else
				{
					if (Type.bSRVOnly)
					{
						// Texture used only as a SRV.
						// Don't bother tracking state for this resource.
						DefaultResourceState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
						bRequiresResourceStateTracking = false;
						return;
					}
				}
			}
	};
}