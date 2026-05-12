#include "D3D12/D3D12State.h"

namespace RenderCore
{
	static D3D12_TEXTURE_ADDRESS_MODE TranslateAddressMode(ESamplerAddressMode AddressMode)
	{
		switch (AddressMode)
		{
		case AM_Clamp: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		case AM_Mirror: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		case AM_Border: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		};
	}

	static D3D12_CULL_MODE TranslateCullMode(ERasterizerCullMode CullMode)
	{
		switch (CullMode)
		{
		case CM_CW: return D3D12_CULL_MODE_BACK;
		case CM_CCW: return D3D12_CULL_MODE_FRONT;
		default: return D3D12_CULL_MODE_NONE;
		};
	}

	static ERasterizerCullMode ReverseTranslateCullMode(D3D12_CULL_MODE CullMode)
	{
		switch (CullMode)
		{
		case D3D12_CULL_MODE_BACK: return CM_CW;
		case D3D12_CULL_MODE_FRONT: return CM_CCW;
		default: return CM_None;
		}
	}

	static D3D12_FILL_MODE TranslateFillMode(ERasterizerFillMode FillMode)
	{
		switch (FillMode)
		{
		case FM_Wireframe: return D3D12_FILL_MODE_WIREFRAME;
		default: return D3D12_FILL_MODE_SOLID;
		};
	}

	static ERasterizerFillMode ReverseTranslateFillMode(D3D12_FILL_MODE FillMode)
	{
		switch (FillMode)
		{
		case D3D12_FILL_MODE_WIREFRAME: return FM_Wireframe;
		default: return FM_Solid;
		}
	}

	static D3D12_COMPARISON_FUNC TranslateCompareFunction(ECompareFunction CompareFunction)
	{
		switch (CompareFunction)
		{
		case CF_Less: return D3D12_COMPARISON_FUNC_LESS;
		case CF_LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
		case CF_Greater: return D3D12_COMPARISON_FUNC_GREATER;
		case CF_GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		case CF_Equal: return D3D12_COMPARISON_FUNC_EQUAL;
		case CF_NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
		case CF_Never: return D3D12_COMPARISON_FUNC_NEVER;
		default: return D3D12_COMPARISON_FUNC_ALWAYS;
		};
	}

	static ECompareFunction ReverseTranslateCompareFunction(D3D12_COMPARISON_FUNC CompareFunction)
	{
		switch (CompareFunction)
		{
		case D3D12_COMPARISON_FUNC_LESS: return CF_Less;
		case D3D12_COMPARISON_FUNC_LESS_EQUAL: return CF_LessEqual;
		case D3D12_COMPARISON_FUNC_GREATER: return CF_Greater;
		case D3D12_COMPARISON_FUNC_GREATER_EQUAL: return CF_GreaterEqual;
		case D3D12_COMPARISON_FUNC_EQUAL: return CF_Equal;
		case D3D12_COMPARISON_FUNC_NOT_EQUAL: return CF_NotEqual;
		case D3D12_COMPARISON_FUNC_NEVER: return CF_Never;
		default: return CF_Always;
		}
	}

	static D3D12_COMPARISON_FUNC TranslateSamplerCompareFunction(ESamplerCompareFunction SamplerComparisonFunction)
	{
		switch (SamplerComparisonFunction)
		{
		case SCF_Less: return D3D12_COMPARISON_FUNC_LESS;
		case SCF_LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
		case SCF_Never:
		default: return D3D12_COMPARISON_FUNC_NEVER;
		};
	}

	static D3D12_STENCIL_OP TranslateStencilOp(EStencilOp StencilOp)
	{
		switch (StencilOp)
		{
		case SO_Zero: return D3D12_STENCIL_OP_ZERO;
		case SO_Replace: return D3D12_STENCIL_OP_REPLACE;
		case SO_SaturatedIncrement: return D3D12_STENCIL_OP_INCR_SAT;
		case SO_SaturatedDecrement: return D3D12_STENCIL_OP_DECR_SAT;
		case SO_Invert: return D3D12_STENCIL_OP_INVERT;
		case SO_Increment: return D3D12_STENCIL_OP_INCR;
		case SO_Decrement: return D3D12_STENCIL_OP_DECR;
		default: return D3D12_STENCIL_OP_KEEP;
		};
	}

	static EStencilOp ReverseTranslateStencilOp(D3D12_STENCIL_OP StencilOp)
	{
		switch (StencilOp)
		{
		case D3D12_STENCIL_OP_ZERO: return SO_Zero;
		case D3D12_STENCIL_OP_REPLACE: return SO_Replace;
		case D3D12_STENCIL_OP_INCR_SAT: return SO_SaturatedIncrement;
		case D3D12_STENCIL_OP_DECR_SAT: return SO_SaturatedDecrement;
		case D3D12_STENCIL_OP_INVERT: return SO_Invert;
		case D3D12_STENCIL_OP_INCR: return SO_Increment;
		case D3D12_STENCIL_OP_DECR: return SO_Decrement;
		default: return SO_Keep;
		};
	}

	static D3D12_BLEND_OP TranslateBlendOp(EBlendOperation BlendOp)
	{
		switch (BlendOp)
		{
		case BO_Subtract: return D3D12_BLEND_OP_SUBTRACT;
		case BO_Min: return D3D12_BLEND_OP_MIN;
		case BO_Max: return D3D12_BLEND_OP_MAX;
		case BO_ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
		default: return D3D12_BLEND_OP_ADD;
		};
	}

	static EBlendOperation ReverseTranslateBlendOp(D3D12_BLEND_OP BlendOp)
	{
		switch (BlendOp)
		{
		case D3D12_BLEND_OP_SUBTRACT: return BO_Subtract;
		case D3D12_BLEND_OP_MIN: return BO_Min;
		case D3D12_BLEND_OP_MAX: return BO_Max;
		case D3D12_BLEND_OP_REV_SUBTRACT: return BO_ReverseSubtract;
		default: return BO_Add;
		};
	}

	static D3D12_BLEND TranslateBlendFactor(EBlendFactor BlendFactor)
	{
		switch (BlendFactor)
		{
		case BF_One: return D3D12_BLEND_ONE;
		case BF_SourceColor: return D3D12_BLEND_SRC_COLOR;
		case BF_InverseSourceColor: return D3D12_BLEND_INV_SRC_COLOR;
		case BF_SourceAlpha: return D3D12_BLEND_SRC_ALPHA;
		case BF_InverseSourceAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
		case BF_DestAlpha: return D3D12_BLEND_DEST_ALPHA;
		case BF_InverseDestAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
		case BF_DestColor: return D3D12_BLEND_DEST_COLOR;
		case BF_InverseDestColor: return D3D12_BLEND_INV_DEST_COLOR;
		case BF_ConstantBlendFactor: return D3D12_BLEND_BLEND_FACTOR;
		case BF_InverseConstantBlendFactor: return D3D12_BLEND_INV_BLEND_FACTOR;
		default: return D3D12_BLEND_ZERO;
		};
	}

	static EBlendFactor ReverseTranslateBlendFactor(D3D12_BLEND BlendFactor)
	{
		switch (BlendFactor)
		{
		case D3D12_BLEND_ONE: return BF_One;
		case D3D12_BLEND_SRC_COLOR: return BF_SourceColor;
		case D3D12_BLEND_INV_SRC_COLOR: return BF_InverseSourceColor;
		case D3D12_BLEND_SRC_ALPHA: return BF_SourceAlpha;
		case D3D12_BLEND_INV_SRC_ALPHA: return BF_InverseSourceAlpha;
		case D3D12_BLEND_DEST_ALPHA: return BF_DestAlpha;
		case D3D12_BLEND_INV_DEST_ALPHA: return BF_InverseDestAlpha;
		case D3D12_BLEND_DEST_COLOR: return BF_DestColor;
		case D3D12_BLEND_INV_DEST_COLOR: return BF_InverseDestColor;
		case D3D12_BLEND_BLEND_FACTOR: return BF_ConstantBlendFactor;
		case D3D12_BLEND_INV_BLEND_FACTOR: return BF_InverseConstantBlendFactor;
		default: return BF_Zero;
		};
	}

	bool operator==(const D3D12_SAMPLER_DESC& lhs, const D3D12_SAMPLER_DESC& rhs)
	{
		return 0 == memcmp(&lhs, &rhs, sizeof(lhs));
	}

	uint32_t GetTypeHash(const D3D12_SAMPLER_DESC& Desc)
	{
		return Desc.Filter;
	}

	bool D3D12SamplerState::CreateSamplerState(const SamplerStateInitializerRHI& Initializer)
	{
		ZeroMemory(&SamplerDesc, sizeof(SamplerDesc));
		SamplerDesc.AddressU = TranslateAddressMode(Initializer.AddressU);
		SamplerDesc.AddressV = TranslateAddressMode(Initializer.AddressV);
		SamplerDesc.AddressW = TranslateAddressMode(Initializer.AddressW);
		SamplerDesc.MipLODBias = Initializer.MipBias;
		SamplerDesc.MaxAnisotropy = ComputeAnisotropyRT(Initializer.MaxAnisotropy);
		SamplerDesc.MinLOD = Initializer.MinMipLevel;
		SamplerDesc.MaxLOD = Initializer.MaxMipLevel;
		// Determine whether we should use one of the comparison modes
		const bool bComparisonEnabled = Initializer.SamplerComparisonFunction != SCF_Never;
		switch (Initializer.Filter)
		{
		case SF_AnisotropicLinear:
		case SF_AnisotropicPoint:
			if (SamplerDesc.MaxAnisotropy == 1)
			{
				SamplerDesc.Filter = bComparisonEnabled ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			}
			else
			{
				// D3D12  doesn't allow using point filtering for mip filter when using anisotropic filtering
				SamplerDesc.Filter = bComparisonEnabled ? D3D12_FILTER_COMPARISON_ANISOTROPIC : D3D12_FILTER_ANISOTROPIC;
			}

			break;
		case SF_Trilinear:
			SamplerDesc.Filter = bComparisonEnabled ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			break;
		case SF_Bilinear:
			SamplerDesc.Filter = bComparisonEnabled ? D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT : D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			break;
		case SF_Point:
			SamplerDesc.Filter = bComparisonEnabled ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_POINT;
			break;
		}
		SamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		SamplerDesc.ComparisonFunc = TranslateSamplerCompareFunction(Initializer.SamplerComparisonFunction);
		return true;
	}

	const D3D12_STATIC_SAMPLER_DESC& D3D12SamplerState::GetSampleDesc() const
	{
		return SamplerDesc;
	}

	bool D3D12RasterizerState::CreateRasterizerState(const RasterizerStateInitializerRHI& Initializer)
	{
		ZeroMemory(&RasterizerDesc, sizeof(RasterizerDesc));
		RasterizerDesc.CullMode = TranslateCullMode(Initializer.CullMode);
		RasterizerDesc.FillMode = TranslateFillMode(Initializer.FillMode);
		RasterizerDesc.SlopeScaledDepthBias = Initializer.SlopeScaleDepthBias;
		RasterizerDesc.FrontCounterClockwise = Initializer.bFrontCounterClockwise;
		RasterizerDesc.DepthBias = math::FloorToInt(Initializer.DepthBias * (float)(1 << 24));
		RasterizerDesc.DepthClipEnable = true;
		RasterizerDesc.MultisampleEnable = Initializer.bAllowMSAA;
		return true;
	}

	const D3D12_RASTERIZER_DESC& D3D12RasterizerState::GetRasterizerDesc() const
	{
		return RasterizerDesc;
	}

	bool D3D12BlendState::CreateBlendState(const BlendStateInitializerRHI& Initializer)
	{
		ZeroMemory(&BlendDesc, sizeof(D3D12_BLEND_DESC));

		BlendDesc.AlphaToCoverageEnable = false;
		BlendDesc.IndependentBlendEnable = Initializer.bUseIndependentRenderTargetBlendStates;

		static_assert(MaxSimultaneousRenderTargets <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT, "Too many MRTs.");
		for (uint32_t RenderTargetIndex = 0; RenderTargetIndex < MaxSimultaneousRenderTargets; ++RenderTargetIndex)
		{
			const BlendStateInitializerRHI::FRenderTarget& RenderTargetInitializer = Initializer.RenderTargets[RenderTargetIndex];
			D3D12_RENDER_TARGET_BLEND_DESC& RenderTarget = BlendDesc.RenderTarget[RenderTargetIndex];
			RenderTarget.BlendEnable =
				RenderTargetInitializer.ColorBlendOp != BO_Add || RenderTargetInitializer.ColorDestBlend != BF_Zero || RenderTargetInitializer.ColorSrcBlend != BF_One ||
				RenderTargetInitializer.AlphaBlendOp != BO_Add || RenderTargetInitializer.AlphaDestBlend != BF_Zero || RenderTargetInitializer.AlphaSrcBlend != BF_One;
			RenderTarget.BlendOp = TranslateBlendOp(RenderTargetInitializer.ColorBlendOp);
			RenderTarget.SrcBlend = TranslateBlendFactor(RenderTargetInitializer.ColorSrcBlend);
			RenderTarget.DestBlend = TranslateBlendFactor(RenderTargetInitializer.ColorDestBlend);
			RenderTarget.BlendOpAlpha = TranslateBlendOp(RenderTargetInitializer.AlphaBlendOp);
			RenderTarget.SrcBlendAlpha = TranslateBlendFactor(RenderTargetInitializer.AlphaSrcBlend);
			RenderTarget.DestBlendAlpha = TranslateBlendFactor(RenderTargetInitializer.AlphaDestBlend);
			RenderTarget.RenderTargetWriteMask =
				((RenderTargetInitializer.ColorWriteMask & CW_RED) ? D3D12_COLOR_WRITE_ENABLE_RED : 0)
				| ((RenderTargetInitializer.ColorWriteMask & CW_GREEN) ? D3D12_COLOR_WRITE_ENABLE_GREEN : 0)
				| ((RenderTargetInitializer.ColorWriteMask & CW_BLUE) ? D3D12_COLOR_WRITE_ENABLE_BLUE : 0)
				| ((RenderTargetInitializer.ColorWriteMask & CW_ALPHA) ? D3D12_COLOR_WRITE_ENABLE_ALPHA : 0);
		}

		return true;
	}

	const D3D12_BLEND_DESC& D3D12BlendState::GetBlendDesc() const
	{
		return BlendDesc;
	}

	bool D3D12DepthStencilState::CreateDepthStencilState(const DepthStencilStateInitializerRHI& Initializer)
	{
		ZeroMemory(&DepthStencilDesc, sizeof(D3D12_DEPTH_STENCIL_DESC));

		// depth part
		DepthStencilDesc.DepthEnable = Initializer.DepthTest != CF_Always || Initializer.bEnableDepthWrite;
		DepthStencilDesc.DepthWriteMask = Initializer.bEnableDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		DepthStencilDesc.DepthFunc = TranslateCompareFunction(Initializer.DepthTest);

		// stencil part
		DepthStencilDesc.StencilEnable = Initializer.bEnableFrontFaceStencil || Initializer.bEnableBackFaceStencil;
		DepthStencilDesc.StencilReadMask = Initializer.StencilReadMask;
		DepthStencilDesc.StencilWriteMask = Initializer.StencilWriteMask;
		DepthStencilDesc.FrontFace.StencilFunc = TranslateCompareFunction(Initializer.FrontFaceStencilTest);
		DepthStencilDesc.FrontFace.StencilFailOp = TranslateStencilOp(Initializer.FrontFaceStencilFailStencilOp);
		DepthStencilDesc.FrontFace.StencilDepthFailOp = TranslateStencilOp(Initializer.FrontFaceDepthFailStencilOp);
		DepthStencilDesc.FrontFace.StencilPassOp = TranslateStencilOp(Initializer.FrontFacePassStencilOp);
		if (Initializer.bEnableBackFaceStencil)
		{
			DepthStencilDesc.BackFace.StencilFunc = TranslateCompareFunction(Initializer.BackFaceStencilTest);
			DepthStencilDesc.BackFace.StencilFailOp = TranslateStencilOp(Initializer.BackFaceStencilFailStencilOp);
			DepthStencilDesc.BackFace.StencilDepthFailOp = TranslateStencilOp(Initializer.BackFaceDepthFailStencilOp);
			DepthStencilDesc.BackFace.StencilPassOp = TranslateStencilOp(Initializer.BackFacePassStencilOp);
		}
		else
		{
			DepthStencilDesc.BackFace = DepthStencilDesc.FrontFace;
		}
		return true;
	}

	const D3D12_DEPTH_STENCIL_DESC& D3D12DepthStencilState::GetDepthStencilDesc() const
	{
		return DepthStencilDesc;
	}

}