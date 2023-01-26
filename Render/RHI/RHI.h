#pragma once
#include "templates/EnumAsByte.h"
#include "RHI/RHIDefinitions.h"
#include "math/math.h"

namespace RenderCore
{

	inline uint32_t ComputeAnisotropyRT(int32_t InitializerMaxAnisotropy)
	{
		return math::Clamp(InitializerMaxAnisotropy, 1, 16);
	}

	struct SamplerStateInitializerRHI
	{
		SamplerStateInitializerRHI() = default;
		SamplerStateInitializerRHI(
			ESamplerFilter InFilter,
			ESamplerAddressMode InAddressU = AM_Wrap,
			ESamplerAddressMode InAddressV = AM_Wrap,
			ESamplerAddressMode InAddressW = AM_Wrap,
			float InMipBias = 0,
			int32_t InMaxAnisotropy = 0,
			float InMinMipLevel = 0,
			float InMaxMipLevel = FLT_MAX,
			uint32_t InBorderColor = 0,
			/** Only supported in D3D11 */
			ESamplerCompareFunction InSamplerComparisonFunction = SCF_Never
		)
			: Filter(InFilter)
			, AddressU(InAddressU)
			, AddressV(InAddressV)
			, AddressW(InAddressW)
			, MipBias(InMipBias)
			, MinMipLevel(InMinMipLevel)
			, MaxMipLevel(InMaxMipLevel)
			, MaxAnisotropy(InMaxAnisotropy)
			, BorderColor(InBorderColor)
			, SamplerComparisonFunction(InSamplerComparisonFunction)
		{
		}
		TEnumAsByte<ESamplerFilter> Filter;
		TEnumAsByte<ESamplerAddressMode> AddressU;
		TEnumAsByte<ESamplerAddressMode> AddressV;
		TEnumAsByte<ESamplerAddressMode> AddressW;
		float MipBias;
		/** Smallest mip map level that will be used, where 0 is the highest resolution mip level. */
		float MinMipLevel;
		/** Largest mip map level that will be used, where 0 is the highest resolution mip level. */
		float MaxMipLevel;
		int32_t MaxAnisotropy;
		uint32_t BorderColor;
		TEnumAsByte<ESamplerCompareFunction> SamplerComparisonFunction;


		 friend uint32_t GetTypeHash(const SamplerStateInitializerRHI& Initializer);
		 friend bool operator== (const SamplerStateInitializerRHI& A, const SamplerStateInitializerRHI& B);
	};
}