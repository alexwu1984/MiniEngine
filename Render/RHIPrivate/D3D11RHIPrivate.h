#pragma once
#include "win/com_ptr.h"
#include <d3d11.h>
#include <d3d11_4.h>
#include <d3d11_2.h>
#include <dxgi1_3.h>
#include <dxgi1_4.h>
#include <dxgi1_6.h>
#include <delayimp.h>
#include "D3D11StateCachePrivate.h"
#include "D3D11/D3D11CommandContext.h"
#include "RHI/RHIShdader.h"

namespace RenderCore
{
	struct D3D11GlobalStats
	{
		// in bytes, never change after RHI, needed to scale game features
		static int64_t GDedicatedVideoMemory;

		// in bytes, never change after RHI, needed to scale game features
		static int64_t GDedicatedSystemMemory;

		// in bytes, never change after RHI, needed to scale game features
		static int64_t GSharedSystemMemory;

		// In bytes. Never changed after RHI init. Our estimate of the amount of memory that we can use for graphics resources in total.
		static int64_t GTotalGraphicsMemory;
	};

	/** Current texture streaming pool size, in bytes. 0 means unlimited. */
	static int64_t GTexturePoolSize;
#define DX_MAX_MSAA_COUNT 8

	struct D3D11Adapter
	{
		/** -1 if not supported or FindAdpater() wasn't called. Ideally we would store a pointer to IDXGIAdapter but it's unlikely the adpaters change during engine init. */
		int32_t AdapterIndex;
		/** The maximum D3D11 feature level supported. 0 if not supported or FindAdpater() wasn't called */
		D3D_FEATURE_LEVEL MaxSupportedFeatureLevel;

		// constructor
		D3D11Adapter(int32_t InAdapterIndex = -1, D3D_FEATURE_LEVEL InMaxSupportedFeatureLevel = (D3D_FEATURE_LEVEL)0)
			: AdapterIndex(InAdapterIndex)
			, MaxSupportedFeatureLevel(InMaxSupportedFeatureLevel)
		{
		}

		bool IsValid() const
		{
			return MaxSupportedFeatureLevel != (D3D_FEATURE_LEVEL)0 && AdapterIndex >= 0;
		}
	};

	struct D3D11DynamicRHIPrivate
	{

		win32::com_ptr<IDXGIFactory1> DXGIFactory1;
		win32::com_ptr< ID3D11Device> Direct3DDevice;
		win32::com_ptr<ID3D11DeviceContext> Direct3DDeviceIMContext;

		win32::com_ptr<ID3D11Device2>        Direct3DDevice2;
		win32::com_ptr<ID3D11DeviceContext2> Direct3DDeviceIMContext2;

		D3D11Adapter ChosenAdapter;
		// we don't use GetDesc().Description as there is a bug with Optimus where it can report the wrong name
		DXGI_ADAPTER_DESC ChosenDescription;

		/** The feature level of the device. */
		D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;

		D3D11StateCacheBase StateCache;

		std::shared_ptr< D3D11CommandContext> CommandContext;

		/** Optional path for RHIWaitForGpuIdle (Win10+ / recent runtime); lazy-init. */
		win32::com_ptr<ID3D11Fence> GpuIdleFence;
		win32::com_ptr<ID3D11DeviceContext4> GpuIdleContext4;
		UINT64 GpuIdleFenceValue = 0;
		bool bGpuIdleFenceInitAttempted = false;
		bool bGpuIdleFenceUsable = false;

		/** Fallback when ID3D11Fence is unavailable: true GPU idle for world swap / resource teardown (Flush alone is not sufficient). */
		win32::com_ptr<ID3D11Query> GpuIdleEventQuery;

		RHIShaderCache ShaderCache;

		std::unordered_map<std::wstring, std::shared_ptr<RHITexture2D>> TexCaches;
	};

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

	inline  DXGI_FORMAT FindDepthResourceDXGIFormat(DXGI_FORMAT InFormat)
	{
		switch (InFormat)
		{
		case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_D32_FLOAT;
		}
		return InFormat;
	}

	inline const TCHAR* GetD3D11TextureFormatString(DXGI_FORMAT TextureFormat)
	{
		static const TCHAR* EmptyString = TEXT("");
		const TCHAR* TextureFormatText = EmptyString;
#define D3DFORMATCASE(x) case x: TextureFormatText = TEXT(#x); break;
		switch (TextureFormat)
		{
			D3DFORMATCASE(DXGI_FORMAT_R8G8B8A8_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_B8G8R8A8_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_B8G8R8X8_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_BC1_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_BC2_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_BC3_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_BC4_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_R16G16B16A16_FLOAT)
				D3DFORMATCASE(DXGI_FORMAT_R32G32B32A32_FLOAT)
				D3DFORMATCASE(DXGI_FORMAT_UNKNOWN)
				D3DFORMATCASE(DXGI_FORMAT_R8_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
				D3DFORMATCASE(DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS)
				D3DFORMATCASE(DXGI_FORMAT_R32G8X24_TYPELESS)
				D3DFORMATCASE(DXGI_FORMAT_D24_UNORM_S8_UINT)
				D3DFORMATCASE(DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
				D3DFORMATCASE(DXGI_FORMAT_R32_FLOAT)
				D3DFORMATCASE(DXGI_FORMAT_R16G16_UINT)
				D3DFORMATCASE(DXGI_FORMAT_R16G16_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_R16G16_SNORM)
				D3DFORMATCASE(DXGI_FORMAT_R16G16_FLOAT)
				D3DFORMATCASE(DXGI_FORMAT_R32G32_FLOAT)
				D3DFORMATCASE(DXGI_FORMAT_R10G10B10A2_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_R16G16B16A16_UINT)
				D3DFORMATCASE(DXGI_FORMAT_R8G8_SNORM)
				D3DFORMATCASE(DXGI_FORMAT_BC5_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_R1_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_R8G8B8A8_TYPELESS)
				D3DFORMATCASE(DXGI_FORMAT_B8G8R8A8_TYPELESS)
				D3DFORMATCASE(DXGI_FORMAT_BC7_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_BC6H_UF16)
		default: TextureFormatText = EmptyString;
		}
#undef D3DFORMATCASE
		return TextureFormatText;
	}

	inline uint32_t GetMaxMSAAQuality(ID3D11Device* device, DXGI_FORMAT InFormat,uint32_t SampleCount)
	{
		if (SampleCount <= DX_MAX_MSAA_COUNT)
		{
			uint32_t Level;
			device->CheckMultisampleQualityLevels(InFormat, SampleCount, &Level);
			return Level > SampleCount ? SampleCount : Level-1;
		}
		// not supported
		return 0xffffffff;
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

}