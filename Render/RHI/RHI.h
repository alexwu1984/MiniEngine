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

	struct RasterizerStateInitializerRHI
	{
		TEnumAsByte<ERasterizerFillMode> FillMode{ ERasterizerFillMode::FM_Solid };
		TEnumAsByte<ERasterizerCullMode> CullMode{ ERasterizerCullMode::CM_CW };
		float DepthBias{ 0.f };
		float SlopeScaleDepthBias{ 0.f };
		bool bAllowMSAA{ false };
		bool bEnableLineAA{ false };

		friend uint32_t GetTypeHash(const RasterizerStateInitializerRHI& Initializer);
	    friend bool operator== (const RasterizerStateInitializerRHI& A, const RasterizerStateInitializerRHI& B);
	};

	struct DepthStencilStateInitializerRHI
	{
		bool bEnableDepthWrite;
		TEnumAsByte<ECompareFunction> DepthTest;

		bool bEnableFrontFaceStencil;
		TEnumAsByte<ECompareFunction> FrontFaceStencilTest;
		TEnumAsByte<EStencilOp> FrontFaceStencilFailStencilOp;
		TEnumAsByte<EStencilOp> FrontFaceDepthFailStencilOp;
		TEnumAsByte<EStencilOp> FrontFacePassStencilOp;
		bool bEnableBackFaceStencil;
		TEnumAsByte<ECompareFunction> BackFaceStencilTest;
		TEnumAsByte<EStencilOp> BackFaceStencilFailStencilOp;
		TEnumAsByte<EStencilOp> BackFaceDepthFailStencilOp;
		TEnumAsByte<EStencilOp> BackFacePassStencilOp;
		uint8_t StencilReadMask;
		uint8_t StencilWriteMask;

		DepthStencilStateInitializerRHI(
			bool bInEnableDepthWrite = true,
			ECompareFunction InDepthTest = CF_LessEqual,
			bool bInEnableFrontFaceStencil = false,
			ECompareFunction InFrontFaceStencilTest = CF_Always,
			EStencilOp InFrontFaceStencilFailStencilOp = SO_Keep,
			EStencilOp InFrontFaceDepthFailStencilOp = SO_Keep,
			EStencilOp InFrontFacePassStencilOp = SO_Keep,
			bool bInEnableBackFaceStencil = false,
			ECompareFunction InBackFaceStencilTest = CF_Always,
			EStencilOp InBackFaceStencilFailStencilOp = SO_Keep,
			EStencilOp InBackFaceDepthFailStencilOp = SO_Keep,
			EStencilOp InBackFacePassStencilOp = SO_Keep,
			uint8_t InStencilReadMask = 0xFF,
			uint8_t InStencilWriteMask = 0xFF
		)
			: bEnableDepthWrite(bInEnableDepthWrite)
			, DepthTest(InDepthTest)
			, bEnableFrontFaceStencil(bInEnableFrontFaceStencil)
			, FrontFaceStencilTest(InFrontFaceStencilTest)
			, FrontFaceStencilFailStencilOp(InFrontFaceStencilFailStencilOp)
			, FrontFaceDepthFailStencilOp(InFrontFaceDepthFailStencilOp)
			, FrontFacePassStencilOp(InFrontFacePassStencilOp)
			, bEnableBackFaceStencil(bInEnableBackFaceStencil)
			, BackFaceStencilTest(InBackFaceStencilTest)
			, BackFaceStencilFailStencilOp(InBackFaceStencilFailStencilOp)
			, BackFaceDepthFailStencilOp(InBackFaceDepthFailStencilOp)
			, BackFacePassStencilOp(InBackFacePassStencilOp)
			, StencilReadMask(InStencilReadMask)
			, StencilWriteMask(InStencilWriteMask)
		{}


		 friend uint32_t GetTypeHash(const DepthStencilStateInitializerRHI& Initializer);
		 friend bool operator== (const DepthStencilStateInitializerRHI& A, const DepthStencilStateInitializerRHI& B);
	};

	class BlendStateInitializerRHI
	{
	public:

		struct FRenderTarget
		{
			enum
			{
				NUM_STRING_FIELDS = 7
			};
			TEnumAsByte<EBlendOperation> ColorBlendOp;
			TEnumAsByte<EBlendFactor> ColorSrcBlend;
			TEnumAsByte<EBlendFactor> ColorDestBlend;
			TEnumAsByte<EBlendOperation> AlphaBlendOp;
			TEnumAsByte<EBlendFactor> AlphaSrcBlend;
			TEnumAsByte<EBlendFactor> AlphaDestBlend;
			TEnumAsByte<EColorWriteMask> ColorWriteMask;

			FRenderTarget(
				EBlendOperation InColorBlendOp = BO_Add,
				EBlendFactor InColorSrcBlend = BF_One,
				EBlendFactor InColorDestBlend = BF_Zero,
				EBlendOperation InAlphaBlendOp = BO_Add,
				EBlendFactor InAlphaSrcBlend = BF_One,
				EBlendFactor InAlphaDestBlend = BF_Zero,
				EColorWriteMask InColorWriteMask = CW_RGBA
			)
				: ColorBlendOp(InColorBlendOp)
				, ColorSrcBlend(InColorSrcBlend)
				, ColorDestBlend(InColorDestBlend)
				, AlphaBlendOp(InAlphaBlendOp)
				, AlphaSrcBlend(InAlphaSrcBlend)
				, AlphaDestBlend(InAlphaDestBlend)
				, ColorWriteMask(InColorWriteMask)
			{}
		};

		BlendStateInitializerRHI() {}

		BlendStateInitializerRHI(const FRenderTarget& InRenderTargetBlendState, bool bInUseAlphaToCoverage = false)
			: bUseIndependentRenderTargetBlendStates(false)
			, bUseAlphaToCoverage(bInUseAlphaToCoverage)
		{
			RenderTargets[0] = InRenderTargetBlendState;
		}

		template<uint32_t NumRenderTargets>
		BlendStateInitializerRHI(const std::array<FRenderTarget, NumRenderTargets>& InRenderTargetBlendStates, bool bInUseAlphaToCoverage = false)
			: bUseIndependentRenderTargetBlendStates(NumRenderTargets > 1)
			, bUseAlphaToCoverage(bInUseAlphaToCoverage)
		{
			static_assert(NumRenderTargets <= MaxSimultaneousRenderTargets, "Too many render target blend states.");

			for (uint32_t RenderTargetIndex = 0; RenderTargetIndex < NumRenderTargets; ++RenderTargetIndex)
			{
				RenderTargets[RenderTargetIndex] = InRenderTargetBlendStates[RenderTargetIndex];
			}
		}

		std::array<FRenderTarget, MaxSimultaneousRenderTargets> RenderTargets;
		bool bUseIndependentRenderTargetBlendStates;
		bool bUseAlphaToCoverage;


		friend uint32_t GetTypeHash(const BlendStateInitializerRHI::FRenderTarget& RenderTarget);
		friend bool operator== (const BlendStateInitializerRHI::FRenderTarget& A, const BlendStateInitializerRHI::FRenderTarget& B);

		friend uint32_t GetTypeHash(const BlendStateInitializerRHI& Initializer);
		friend bool operator== (const BlendStateInitializerRHI& A, const BlendStateInitializerRHI& B);

	};
}