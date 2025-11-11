#pragma once
#include "win/win32.h"
#include "win/com_ptr.h"
#include "d3dx12.h"
#include <dxgi1_4.h>
#include <dxgi1_5.h>
#include <delayimp.h>
#include "RHI/RHIDefinitions.h"
#include "D3D12/D3D12RHICommon.h"

namespace RenderCore
{
#define MAX_SRVS		48
#define MAX_SAMPLERS	16
#define MAX_UAVS		16
#define MAX_CBS			16
#define MAX_ROOT_CBVS	MAX_CBS
#define WINDOWS_DEFAULT_NUM_BACK_BUFFERS 3

#define FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
#define FD3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL      ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN   ((D3D12_GPU_VIRTUAL_ADDRESS)-1)
#define D3D12_CPU_VIRTUAL_ADDRESS_UNKNOWN	((SIZE_T)-1)

	typedef uint16_t CBVSlotMask;
	static_assert(MAX_ROOT_CBVS <= MAX_CBS, "MAX_ROOT_CBVS must be <= MAX_CBS.");
	static_assert((8 * sizeof(CBVSlotMask)) >= MAX_CBS, "CBVSlotMask isn't large enough to cover all CBs. Please increase the size.");
	static_assert((8 * sizeof(CBVSlotMask)) >= MAX_ROOT_CBVS, "CBVSlotMask isn't large enough to cover all CBs. Please increase the size.");
	static const CBVSlotMask GRootCBVSlotMask = (1 << MAX_ROOT_CBVS) - 1; // Mask for all slots that are used by root descriptors.
	static const CBVSlotMask GDescriptorTableCBVSlotMask = static_cast<CBVSlotMask>(-1) & ~(GRootCBVSlotMask); // Mask for all slots that are used by a root descriptor table.

#if MAX_SRVS > 32
	typedef uint64_t SRVSlotMask;
#else
	typedef uint32_t SRVSlotMask;
#endif
	static_assert((8 * sizeof(SRVSlotMask)) >= MAX_SRVS, "SRVSlotMask isn't large enough to cover all SRVs. Please increase the size.");

	typedef uint16_t SamplerSlotMask;
	static_assert((8 * sizeof(SamplerSlotMask)) >= MAX_SAMPLERS, "SamplerSlotMask isn't large enough to cover all Samplers. Please increase the size.");

	typedef uint16_t UAVSlotMask;
	static_assert((8 * sizeof(UAVSlotMask)) >= MAX_UAVS, "UAVSlotMask isn't large enough to cover all UAVs. Please increase the size.");

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

	/** Find an appropriate DXGI format for the input format and SRGB setting. */
	inline DXGI_FORMAT FindShaderResourceDXGIFormat(DXGI_FORMAT InFormat, bool bSRGB)
	{
		if (bSRGB)
		{
			switch (InFormat)
			{
			case DXGI_FORMAT_B8G8R8A8_TYPELESS:    return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			case DXGI_FORMAT_BC1_TYPELESS:         return DXGI_FORMAT_BC1_UNORM_SRGB;
			case DXGI_FORMAT_BC2_TYPELESS:         return DXGI_FORMAT_BC2_UNORM_SRGB;
			case DXGI_FORMAT_BC3_TYPELESS:         return DXGI_FORMAT_BC3_UNORM_SRGB;
			case DXGI_FORMAT_BC7_TYPELESS:         return DXGI_FORMAT_BC7_UNORM_SRGB;
			};
		}
		else
		{
			switch (InFormat)
			{
			case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
			case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
			case DXGI_FORMAT_BC1_TYPELESS:      return DXGI_FORMAT_BC1_UNORM;
			case DXGI_FORMAT_BC2_TYPELESS:      return DXGI_FORMAT_BC2_UNORM;
			case DXGI_FORMAT_BC3_TYPELESS:      return DXGI_FORMAT_BC3_UNORM;
			case DXGI_FORMAT_BC7_TYPELESS:      return DXGI_FORMAT_BC7_UNORM;
			};
		}
		switch (InFormat)
		{
		case DXGI_FORMAT_R24G8_TYPELESS: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_R32_FLOAT;
		case DXGI_FORMAT_R16_TYPELESS: return DXGI_FORMAT_R16_UNORM;
			// Changing Depth Buffers to 32 bit on Dingo as D24S8 is actually implemented as a 32 bit buffer in the hardware
		case DXGI_FORMAT_R32G8X24_TYPELESS: return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
		}
		return InFormat;
	}

	/** Find an appropriate DXGI format for the input format and SRGB setting. */
	inline DXGI_FORMAT FindSharedResourceDXGIFormat(DXGI_FORMAT InFormat, bool bSRGB)
	{
		if (bSRGB)
		{
			switch (InFormat)
			{
			case DXGI_FORMAT_B8G8R8X8_TYPELESS:    return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
			case DXGI_FORMAT_B8G8R8A8_TYPELESS:    return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			case DXGI_FORMAT_BC1_TYPELESS:         return DXGI_FORMAT_BC1_UNORM_SRGB;
			case DXGI_FORMAT_BC2_TYPELESS:         return DXGI_FORMAT_BC2_UNORM_SRGB;
			case DXGI_FORMAT_BC3_TYPELESS:         return DXGI_FORMAT_BC3_UNORM_SRGB;
			case DXGI_FORMAT_BC7_TYPELESS:         return DXGI_FORMAT_BC7_UNORM_SRGB;
			};
		}
		else
		{
			switch (InFormat)
			{
			case DXGI_FORMAT_B8G8R8X8_TYPELESS:    return DXGI_FORMAT_B8G8R8X8_UNORM;
			case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
			case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
			case DXGI_FORMAT_BC1_TYPELESS:      return DXGI_FORMAT_BC1_UNORM;
			case DXGI_FORMAT_BC2_TYPELESS:      return DXGI_FORMAT_BC2_UNORM;
			case DXGI_FORMAT_BC3_TYPELESS:      return DXGI_FORMAT_BC3_UNORM;
			case DXGI_FORMAT_BC7_TYPELESS:      return DXGI_FORMAT_BC7_UNORM;
			};
		}
		switch (InFormat)
		{
		case DXGI_FORMAT_R32G32B32A32_TYPELESS: return DXGI_FORMAT_R32G32B32A32_UINT;
		case DXGI_FORMAT_R32G32B32_TYPELESS:    return DXGI_FORMAT_R32G32B32_UINT;
		case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_UNORM;
		case DXGI_FORMAT_R32G32_TYPELESS:       return DXGI_FORMAT_R32G32_UINT;
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:  return DXGI_FORMAT_R10G10B10A2_UNORM;
		case DXGI_FORMAT_R16G16_TYPELESS:       return DXGI_FORMAT_R16G16_UNORM;
		case DXGI_FORMAT_R8G8_TYPELESS:         return DXGI_FORMAT_R8G8_UNORM;
		case DXGI_FORMAT_R8_TYPELESS:           return DXGI_FORMAT_R8_UNORM;

		case DXGI_FORMAT_BC4_TYPELESS:         return DXGI_FORMAT_BC4_UNORM;
		case DXGI_FORMAT_BC5_TYPELESS:         return DXGI_FORMAT_BC5_UNORM;



		case DXGI_FORMAT_R24G8_TYPELESS: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_R32_FLOAT;
		case DXGI_FORMAT_R16_TYPELESS: return DXGI_FORMAT_R16_UNORM;
			// Changing Depth Buffers to 32 bit on Dingo as D24S8 is actually implemented as a 32 bit buffer in the hardware
		case DXGI_FORMAT_R32G8X24_TYPELESS: return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
		}
		return InFormat;
	}

	inline DXGI_FORMAT GetPlatformTextureResourceFormat(DXGI_FORMAT InFormat, uint32_t InFlags)
	{
		// Find valid shared texture format
		if (InFlags & TexCreate_Shared)
		{
			return FindSharedResourceDXGIFormat(InFormat, InFlags & TexCreate_SRGB);
		}
		return FindShaderResourceDXGIFormat(InFormat, InFlags & TexCreate_SRGB);
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

	/** Find the appropriate depth-stencil targetable DXGI format for the given format. */
	inline DXGI_FORMAT FindDepthStencilDXGIFormat(DXGI_FORMAT DSVFormat)
	{
		switch (DSVFormat)
		{
			// 32-bit Z w/ Stencil
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

			// No Stencil
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
			return DXGI_FORMAT_R32_FLOAT;

			// 24-bit Z
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

			// 16-bit Z w/o Stencil
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
			return DXGI_FORMAT_R16_UNORM;

		default:
			return DXGI_FORMAT_UNKNOWN;
		}
	}

	inline DXGI_FORMAT GetRenderTargetFormat(EPixelFormat PixelFormat)
	{
		DXGI_FORMAT	DXFormat = (DXGI_FORMAT)GPixelFormats[PixelFormat].PlatformFormat;
		switch (DXFormat)
		{
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:		return DXGI_FORMAT_B8G8R8A8_UNORM;
		case DXGI_FORMAT_BC1_TYPELESS:			return DXGI_FORMAT_BC1_UNORM;
		case DXGI_FORMAT_BC2_TYPELESS:			return DXGI_FORMAT_BC2_UNORM;
		case DXGI_FORMAT_BC3_TYPELESS:			return DXGI_FORMAT_BC3_UNORM;
		case DXGI_FORMAT_R16_TYPELESS:			return DXGI_FORMAT_R16_UNORM;
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:		return DXGI_FORMAT_R8G8B8A8_UNORM;
		default: 								return DXFormat;
		}
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
			Assert(Before != After);
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
				Assert(pCommandList);
				pCommandList->ResourceBarrier((UINT)Barriers.size(), Barriers.data());
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

	inline bool IsCPUWritable(D3D12_HEAP_TYPE HeapType, const D3D12_HEAP_PROPERTIES* pCustomHeapProperties = nullptr)
	{
		assert(HeapType == D3D12_HEAP_TYPE_CUSTOM ? pCustomHeapProperties != nullptr : true);
		return HeapType == D3D12_HEAP_TYPE_UPLOAD ||
			(HeapType == D3D12_HEAP_TYPE_CUSTOM &&
				(pCustomHeapProperties->CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE || pCustomHeapProperties->CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_BACK));
	}

	inline bool IsCPUInaccessible(D3D12_HEAP_TYPE HeapType, const D3D12_HEAP_PROPERTIES* pCustomHeapProperties = nullptr)
	{
		assert(HeapType == D3D12_HEAP_TYPE_CUSTOM ? pCustomHeapProperties != nullptr : true);
		return HeapType == D3D12_HEAP_TYPE_DEFAULT ||
			(HeapType == D3D12_HEAP_TYPE_CUSTOM &&
				(pCustomHeapProperties->CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE));
	}

	inline D3D12_RESOURCE_STATES DetermineInitialResourceState(D3D12_HEAP_TYPE HeapType, const D3D12_HEAP_PROPERTIES* pCustomHeapProperties = nullptr)
	{
		if (HeapType == D3D12_HEAP_TYPE_DEFAULT || IsCPUWritable(HeapType, pCustomHeapProperties))
		{
			return D3D12_RESOURCE_STATE_GENERIC_READ;
		}
		else
		{
			assert(HeapType == D3D12_HEAP_TYPE_READBACK);
			return D3D12_RESOURCE_STATE_COPY_DEST;
		}
	}

	inline D3D12_RESOURCE_FLAGS CombineResourceFlags(int32_t TexFlags)
	{
		D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE;

		if (TexFlags & TexCreate_DepthStencilTargetable)
		{
			Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		}

		if (TexFlags & TexCreate_UAV)
		{
			Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}

		if (TexFlags & TexCreate_RenderTargetable)
		{
			Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		}			

		return Flags;
	}

	inline D3D12_RESOURCE_DESC DescribeTex2D(uint32_t Width, uint32_t Height, uint32_t DepthOrArraySize, uint32_t NumMips, DXGI_FORMAT Format, UINT Flags)
	{
		D3D12_RESOURCE_DESC Desc = {};
		Desc.Alignment = 0;
		Desc.DepthOrArraySize = (UINT16)DepthOrArraySize;
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		Desc.Flags = (D3D12_RESOURCE_FLAGS)Flags;
		Desc.Format = Format;
		Desc.Width = (UINT)Width;
		Desc.Height = (UINT)Height;
		Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		Desc.MipLevels = (UINT16)NumMips;
		Desc.SampleDesc.Count = 1;
		Desc.SampleDesc.Quality = 0;

		return Desc;
	}

	inline uint32_t ComputeNumMips(uint32_t Width, uint32_t Height)
	{
		uint32_t HighBit;
		_BitScanReverse((unsigned long*)&HighBit, Width | Height);
		return HighBit + 1;
	}

#define GET_QUEUE_TYPE(f) ((D3D12_COMMAND_LIST_TYPE)(f >> 56))

	inline ED3D12CommandQueueType GetCommandQueueType(D3D12_COMMAND_LIST_TYPE Type /*= D3D12_COMMAND_LIST_TYPE_DIRECT*/)
	{
		switch (Type)
		{
		case D3D12_COMMAND_LIST_TYPE_COMPUTE: return ED3D12CommandQueueType::Async;
		case D3D12_COMMAND_LIST_TYPE_COPY: return ED3D12CommandQueueType::Copy;
		default: return ED3D12CommandQueueType::Default;
		}
	}

	inline D3D_PRIMITIVE_TOPOLOGY GetD3D12PrimitiveType(EPrimitiveType PrimitiveType, bool bUsingTessellation)
	{
		if (bUsingTessellation)
		{
			switch (PrimitiveType)
			{
			case PT_1_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
			case PT_2_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST;

				// This is the case for tessellation without AEN or other buffers, so just flip to 3 CPs
			case PT_TriangleList: return D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

			case PT_LineList:
			case PT_TriangleStrip:
			case PT_QuadList:
			case PT_RectList:
			case PT_PointList:
				//UE_LOG(LogD3D12RHI, Fatal, TEXT("Invalid type specified for tessellated render, probably missing a case in FSkeletalMeshSceneProxy::DrawDynamicElementsByMaterial or FStaticMeshSceneProxy::GetMeshElement"));
				break;
			default:
				// Other cases are valid.
				break;
			};
		}

		switch (PrimitiveType)
		{
		case PT_TriangleList: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case PT_TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case PT_LineList: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		case PT_PointList: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
#if defined(D3D12RHI_PRIMITIVE_TOPOLOGY_RECTLIST)
		case PT_RectList: return D3D12RHI_PRIMITIVE_TOPOLOGY_RECTLIST;
#endif

			// ControlPointPatchList types will pretend to be TRIANGLELISTS with a stride of N 
			// (where N is the number of control points specified), so we can return them for
			// tessellation and non-tessellation. This functionality is only used when rendering a 
			// default material with something that claims to be tessellated, generally because the 
			// tessellation material failed to compile for some reason.
		case PT_3_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
		case PT_4_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
		case PT_5_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST;
		case PT_6_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST;
		case PT_7_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST;
		case PT_8_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST;
		case PT_9_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST;
		case PT_10_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST;
		case PT_11_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST;
		case PT_12_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST;
		case PT_13_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST;
		case PT_14_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST;
		case PT_15_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST;
		case PT_16_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST;
		case PT_17_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST;
		case PT_18_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST;
		case PT_19_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST;
		case PT_20_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST;
		case PT_21_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST;
		case PT_22_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST;
		case PT_23_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST;
		case PT_24_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST;
		case PT_25_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST;
		case PT_26_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST;
		case PT_27_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST;
		case PT_28_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST;
		case PT_29_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST;
		case PT_30_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST;
		case PT_31_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST;
		case PT_32_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST;
			//default: UE_LOG(LogD3D12RHI, Fatal, TEXT("Unknown primitive type: %u"), PrimitiveType);
		};

		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}

	inline D3D12_PRIMITIVE_TOPOLOGY_TYPE D3D12PrimitiveTypeToTopologyType(D3D_PRIMITIVE_TOPOLOGY PrimitiveType)
	{
		switch (PrimitiveType)
		{
		case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

		case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
		case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
		case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
		case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

		case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
		case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
		case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
		case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

#if defined(D3D12RHI_PRIMITIVE_TOPOLOGY_RECTLIST)
		case D3D12RHI_PRIMITIVE_TOPOLOGY_RECTLIST:
			return D3D12RHI_PRIMITIVE_TOPOLOGY_TYPE_RECT;
#endif

		case D3D_PRIMITIVE_TOPOLOGY_UNDEFINED:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;

		default:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
		}
	}

}