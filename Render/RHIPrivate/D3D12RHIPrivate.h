#pragma once
#include "win/win32.h"
#include "win/com_ptr.h"
#include "d3dx12.h"
#include <dxgi1_4.h>
#include <dxgi1_5.h>
#include <delayimp.h>

namespace RenderCore
{
#define MAX_SRVS		48
#define MAX_SAMPLERS	16
#define MAX_UAVS		16
#define MAX_CBS			16
#define MAX_ROOT_CBVS	MAX_CBS

#define FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
#define FD3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER

	typedef uint16_t CBVSlotMask;
	static_assert(MAX_ROOT_CBVS <= MAX_CBS, "MAX_ROOT_CBVS must be <= MAX_CBS.");
	static_assert((8 * sizeof(CBVSlotMask)) >= MAX_CBS, "CBVSlotMask isn't large enough to cover all CBs. Please increase the size.");
	static_assert((8 * sizeof(CBVSlotMask)) >= MAX_ROOT_CBVS, "CBVSlotMask isn't large enough to cover all CBs. Please increase the size.");
	static const CBVSlotMask GRootCBVSlotMask = (1 << MAX_ROOT_CBVS) - 1; // Mask for all slots that are used by root descriptors.
	static const CBVSlotMask GDescriptorTableCBVSlotMask = static_cast<CBVSlotMask>(-1) & ~(GRootCBVSlotMask); // Mask for all slots that are used by a root descriptor table.

	enum EShaderVisibility
	{
		SV_Vertex,
		SV_Pixel,
		SV_Hull,
		SV_Domain,
		SV_Geometry,
		SV_All,
		SV_ShaderVisibilityCount
	};

	enum ERTRootSignatureType
	{
		RS_Raster,
		RS_RayTracingGlobal,
		RS_RayTracingLocal,
	};

	struct FShaderRegisterCounts
	{
		uint8_t SamplerCount;
		uint8_t ConstantBufferCount;
		uint8_t ShaderResourceCount;
		uint8_t UnorderedAccessCount;
	};

	// if this changes you need to make sure all D3D11 shaders get invalidated
	struct FShaderCodePackedResourceCounts
	{
		// for FindOptionalData() and AddOptionalData()
		static const uint8_t Key = 'p';

		bool bGlobalUniformBufferUsed;
		uint8_t NumSamplers;
		uint8_t NumSRVs;
		uint8_t NumCBs;
		uint8_t NumUAVs;
	};

	/** This function is used as a SEH filter to catch only delay load exceptions. */
	inline bool IsDelayLoadException(PEXCEPTION_POINTERS ExceptionPointers)
	{
#if WINVER > 0x502	// Windows SDK 7.1 doesn't define VcppException
		switch (ExceptionPointers->ExceptionRecord->ExceptionCode)
		{
		case VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND):
		case VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND):
			return EXCEPTION_EXECUTE_HANDLER;
		default:
			return EXCEPTION_CONTINUE_SEARCH;
		}
#else
		return EXCEPTION_EXECUTE_HANDLER;
#endif
	}

	/**
* Since CreateDXGIFactory is a delay loaded import from the DXGI DLL, if the user
* doesn't have Vista/DX10, calling CreateDXGIFactory will throw an exception.
* We use SEH to detect that case and fail gracefully.
*/
	inline void SafeCreateDXGIFactory(IDXGIFactory4** DXGIFactory)
	{
		__try
		{


			CreateDXGIFactory(__uuidof(IDXGIFactory4), (void**)DXGIFactory);
		}
		__except (IsDelayLoadException(GetExceptionInformation()))
		{
			// We suppress warning C6322: Empty _except block. Appropriate checks are made upon returning. 
		}
	}
	/**
* Returns the minimum D3D feature level required to create based on
* command line parameters.
*/
	static D3D_FEATURE_LEVEL GetRequiredD3DFeatureLevel()
	{
		return D3D_FEATURE_LEVEL_11_0;
	}


	/** Find the appropriate depth-stencil typeless DXGI format for the given format. */
	inline DXGI_FORMAT FindDepthStencilParentDXGIFormat(DXGI_FORMAT InFormat)
	{
		switch (InFormat)
		{
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return DXGI_FORMAT_R24G8_TYPELESS;
			// Changing Depth Buffers to 32 bit on Dingo as D24S8 is actually implemented as a 32 bit buffer in the hardware
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return DXGI_FORMAT_R32G8X24_TYPELESS;
		case DXGI_FORMAT_D32_FLOAT:
			return DXGI_FORMAT_R32_TYPELESS;
		case DXGI_FORMAT_D16_UNORM:
			return DXGI_FORMAT_R16_TYPELESS;
		};
		return InFormat;
	}

	inline uint8_t GetPlaneCount(DXGI_FORMAT Format)
	{
		// Currently, the only planar resources used are depth-stencil formats
		// Note there is a D3D12 helper for this, D3D12GetFormatPlaneCount
		switch (FindDepthStencilParentDXGIFormat(Format))
		{
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_R32G8X24_TYPELESS:
			return 2;
		default:
			return 1;
		}
	}

	static bool D3D12RHI_ShouldCreateWithWarp()
	{
		// Use the warp adapter if specified on the command line.
		static bool bCreateWithWarp = false;
		return bCreateWithWarp;
	}

	static inline int D3D12RHI_PreferAdapterVendor()
	{
		//if (FParse::Param(FCommandLine::Get(), TEXT("preferAMD")))
		//{
		//	return 0x1002;
		//}

		//if (FParse::Param(FCommandLine::Get(), TEXT("preferIntel")))
		//{
		//	return 0x8086;
		//}

		//if (FParse::Param(FCommandLine::Get(), TEXT("preferNvidia")))
		//{
		//	return 0x10DE;
		//}

		return 0x10DE;
	}

	uint32_t SSE4_CRC32(const void* Data, size_t NumBytes);

	class FD3D12Fence;
	class D3D12SyncPoint
	{
	public:
		explicit D3D12SyncPoint()
			: Fence(nullptr)
			, Value(0)
		{
		}

		explicit D3D12SyncPoint(FD3D12Fence* InFence, uint64_t InValue)
			: Fence(InFence)
			, Value(InValue)
		{
		}

		bool IsValid() const;
		bool IsComplete() const;
		void WaitForCompletion() const;

	private:
		FD3D12Fence* Fence;
		uint64_t Value;
	};


	/**
 * The base class of threadsafe reference counted objects.
 */
	template <class Type>
	struct ThreadsafeQueue
	{
	private:
		mutable std::recursive_mutex	SynchronizationObject; // made this mutable so this class can have const functions and still be thread safe
		std::deque<Type>				Items;
		uint32_t						Size = 0;
	public:

		inline const uint32_t GetSize() const { return Size; }

		void Enqueue(const Type& Item)
		{
			std::lock_guard<std::recursive_mutex> ScopeLock(SynchronizationObject);
			Items.push_back(Item);
			Size++;
		}

		bool Dequeue(Type& Result)
		{
			std::lock_guard<std::recursive_mutex> ScopeLock(SynchronizationObject);
			if (Items.empty())
			{
				return false;
			}

			Size--;
			Result = Items.front();
			Items.pop_front();
			return true;
		}

		template <typename CompareFunc>
		bool Dequeue(Type& Result, CompareFunc& Func)
		{
			std::lock_guard<std::recursive_mutex> ScopeLock(SynchronizationObject);

			if (Items.empty())
			{
				return false;
			}

			Result = Items.back();
			if (Func(Result))
			{
				Size--;
				Result = Items.front();
				Items.pop_front();

				return true;
			}
			return false;
		}

		template <typename CompareFunc>
		bool BatchDequeue(std::deque<Type>* Result, CompareFunc& Func, uint32_t MaxItems)
		{
			std::lock_guard<std::recursive_mutex> ScopeLock(SynchronizationObject);

			uint32_t i = 0;
			Type Item;
			while (!Items.empty() && i <= MaxItems)
			{
				Item = Items.back();
				if (Func(Item))
				{
					Size--;
					Result = Items.front();
					Items.pop_front();
					Result->push_back(Item);

					i++;
				}
				else
				{
					break;
				}
			}

			return i > 0;
		}

		bool Peek(Type& Result)
		{
			std::lock_guard<std::recursive_mutex> ScopeLock(SynchronizationObject);
			if (Items.empty())
			{
				return false;
			}

			return Items.back();
		}

		bool IsEmpty()
		{
			std::lock_guard<std::recursive_mutex> ScopeLock(SynchronizationObject);
			return Items.empty();
		}

		void Empty()
		{
			std::lock_guard<std::recursive_mutex> ScopeLock(SynchronizationObject);
			Items.clear();
		}
	};

	class FD3D12ResourceBarrierBatcher 
	{
	public:
		explicit FD3D12ResourceBarrierBatcher()
		{};

		// Add a UAV barrier to the batch. Ignoring the actual resource for now.
		void AddUAV()
		{
			//Barriers.AddUninitialized();
			Barriers.push_back({});
			D3D12_RESOURCE_BARRIER& Barrier = Barriers.back();
			Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			Barrier.UAV.pResource = nullptr;	// Ignore the resource ptr for now. HW doesn't do anything with it.
		}

		// Add a transition resource barrier to the batch.
		void AddTransition(ID3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32_t Subresource)
		{
			assert(Before != After);
			//Barriers.AddUninitialized();
			Barriers.push_back({});
			D3D12_RESOURCE_BARRIER& Barrier = Barriers.back();
			Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			Barrier.Transition.StateBefore = Before;
			Barrier.Transition.StateAfter = After;
			Barrier.Transition.Subresource = Subresource;
			Barrier.Transition.pResource = pResource;
		}

		void AddAliasingBarrier(ID3D12Resource* pResource)
		{
			//Barriers.AddUninitialized();
			Barriers.push_back({});
			D3D12_RESOURCE_BARRIER& Barrier = Barriers.back();
			Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
			Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			Barrier.Aliasing.pResourceBefore = NULL;
			Barrier.Aliasing.pResourceAfter = pResource;
		}

		// Flush the batch to the specified command list then reset.
		void Flush(ID3D12GraphicsCommandList* pCommandList)
		{
			if (Barriers.size())
			{
				assert(pCommandList);
				pCommandList->ResourceBarrier(Barriers.size(), Barriers.data());
				Reset();
			}
		}

		// Clears the batch.
		void Reset()
		{
			Barriers.clear();
			//Barriers.SetNumUnsafeInternal(0);	// Reset the array without shrinking (Does not destruct items, does not de-allocate memory).
			//check(Barriers.Num() == 0);
		}

		const std::vector<D3D12_RESOURCE_BARRIER>& GetBarriers() const
		{
			return Barriers;
		}

	private:
		std::vector<D3D12_RESOURCE_BARRIER> Barriers;
	};

	// Custom resource states
// To Be Determined (TBD) means we need to fill out a resource barrier before the command list is executed.
#define D3D12_RESOURCE_STATE_TBD (D3D12_RESOURCE_STATES)-1
#define D3D12_RESOURCE_STATE_CORRUPT (D3D12_RESOURCE_STATES)-2

	class CResourceState
	{
	public:
		void Initialize(uint32_t SubresourceCount);

		bool AreAllSubresourcesSame() const;
		bool CheckResourceState(D3D12_RESOURCE_STATES State) const;
		bool CheckResourceStateInitalized() const;
		D3D12_RESOURCE_STATES GetSubresourceState(uint32_t SubresourceIndex) const;
		void SetResourceState(D3D12_RESOURCE_STATES State);
		void SetSubresourceState(uint32_t SubresourceIndex, D3D12_RESOURCE_STATES State);

	private:
		// Only used if m_AllSubresourcesSame is 1.
		// Bits defining the state of the full resource, bits are from D3D12_RESOURCE_STATES
		D3D12_RESOURCE_STATES m_ResourceState : 31;

		// Set to 1 if m_ResourceState is valid.  In this case, all subresources have the same state
		// Set to 0 if m_SubresourceState is valid.  In this case, each subresources may have a different state (or may be unknown)
		uint32_t m_AllSubresourcesSame : 1;

		// Only used if m_AllSubresourcesSame is 0.
		// The state of each subresources.  Bits are from D3D12_RESOURCE_STATES.
		std::vector<D3D12_RESOURCE_STATES> m_SubresourceState;
	};


	struct FD3D12QuantizedBoundShaderState
	{
		FShaderRegisterCounts RegisterCounts[SV_ShaderVisibilityCount];
		ERTRootSignatureType RootSignatureType = RS_Raster;
		bool bAllowIAInputLayout;

		inline bool operator==(const FD3D12QuantizedBoundShaderState& RHS) const
		{

			return 0 == memcmp(this, &RHS, sizeof(RHS));
		}

		bool operator()(const FD3D12QuantizedBoundShaderState& _Left, const FD3D12QuantizedBoundShaderState& _Right) const
		{	// apply operator== to operands
			return (GetTypeHash(_Left) < GetTypeHash(_Right));
		}

		static uint32_t GetTypeHash(const FD3D12QuantizedBoundShaderState& Key);

		static void InitShaderRegisterCounts(const D3D12_RESOURCE_BINDING_TIER& ResourceBindingTier, const FShaderCodePackedResourceCounts& Counts, FShaderRegisterCounts& Shader, bool bAllowUAVs = false);
	};

}