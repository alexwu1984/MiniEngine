#pragma once
#include "win/com_ptr.h"
#include <d3d11.h>
#include <dxgi1_3.h>
#include <dxgi1_4.h>
#include <dxgi1_6.h>
#include <delayimp.h>
#include "D3D11StateCachePrivate.h"

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

	struct D3D11DynamicRHIP
	{

		win32::com_ptr<IDXGIFactory1> DXGIFactory1;
		win32::com_ptr< ID3D11Device> Direct3DDevice;
		win32::com_ptr<ID3D11DeviceContext> Direct3DDeviceIMContext;
		D3D11Adapter ChosenAdapter;
		// we don't use GetDesc().Description as there is a bug with Optimus where it can report the wrong name
		DXGI_ADAPTER_DESC ChosenDescription;

		/** The feature level of the device. */
		D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;

		D3D11StateCacheBase StateCache;
	};


}