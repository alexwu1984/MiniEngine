#pragma once
#include "win/win32.h"
#include "win/com_ptr.h"
#include "d3dx12.h"
#include <dxgi1_4.h>
#include <dxgi1_5.h>
#include <delayimp.h>
#include "RHI/RHIDefinitions.h"
#include "D3D12/D3D12RHICommon.h"
#include "D3D12/D3D12Util.h"

namespace RenderCore
{
#define FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
#define FD3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL      ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN   ((D3D12_GPU_VIRTUAL_ADDRESS)-1)
#define D3D12_CPU_VIRTUAL_ADDRESS_UNKNOWN	((SIZE_T)-1)

	/** SEH filter for delay-loaded DXGI. */
	inline bool IsDelayLoadException(PEXCEPTION_POINTERS ExceptionPointers)
	{
#if WINVER > 0x502
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

	inline void SafeCreateDXGIFactory(IDXGIFactory4** DXGIFactory)
	{
		__try
		{
			CreateDXGIFactory(__uuidof(IDXGIFactory4), (void**)DXGIFactory);
		}
		__except (IsDelayLoadException(GetExceptionInformation()))
		{
		}
	}

	static D3D_FEATURE_LEVEL GetRequiredD3DFeatureLevel()
	{
		return D3D_FEATURE_LEVEL_11_0;
	}

	static bool D3D12RHI_ShouldCreateWithWarp()
	{
		static bool bCreateWithWarp = false;
		return bCreateWithWarp;
	}

	static inline int D3D12RHI_PreferAdapterVendor()
	{
		return 0x10DE;
	}

	inline D3D_PRIMITIVE_TOPOLOGY GetD3D12PrimitiveType(EPrimitiveType PrimitiveType, bool bUsingTessellation)
	{
		if (bUsingTessellation)
		{
			switch (PrimitiveType)
			{
			case PT_1_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
			case PT_2_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST;
			case PT_TriangleList: return D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
			case PT_LineList:
			case PT_TriangleStrip:
			case PT_QuadList:
			case PT_RectList:
			case PT_PointList:
				break;
			default:
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
