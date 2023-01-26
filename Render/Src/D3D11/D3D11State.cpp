#include "D3D11/D3D11State.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "core/color.h"
#include "D3D11/D3D11RHI.h"

namespace RenderCore
{

	static D3D11_TEXTURE_ADDRESS_MODE TranslateAddressMode(ESamplerAddressMode AddressMode)
	{
		switch (AddressMode)
		{
		case AM_Clamp: return D3D11_TEXTURE_ADDRESS_CLAMP;
		case AM_Mirror: return D3D11_TEXTURE_ADDRESS_MIRROR;
		case AM_Border: return D3D11_TEXTURE_ADDRESS_BORDER;
		default: return D3D11_TEXTURE_ADDRESS_WRAP;
		};
	}

	static D3D11_CULL_MODE TranslateCullMode(ERasterizerCullMode CullMode)
	{
		switch (CullMode)
		{
		case CM_CW: return D3D11_CULL_BACK;
		case CM_CCW: return D3D11_CULL_FRONT;
		default: return D3D11_CULL_NONE;
		};
	}

	static D3D11_FILL_MODE TranslateFillMode(ERasterizerFillMode FillMode)
	{
		switch (FillMode)
		{
		case FM_Wireframe: return D3D11_FILL_WIREFRAME;
		default: return D3D11_FILL_SOLID;
		};
	}

	static D3D11_COMPARISON_FUNC TranslateCompareFunction(ECompareFunction CompareFunction)
	{
		switch (CompareFunction)
		{
		case CF_Less: return D3D11_COMPARISON_LESS;
		case CF_LessEqual: return D3D11_COMPARISON_LESS_EQUAL;
		case CF_Greater: return D3D11_COMPARISON_GREATER;
		case CF_GreaterEqual: return D3D11_COMPARISON_GREATER_EQUAL;
		case CF_Equal: return D3D11_COMPARISON_EQUAL;
		case CF_NotEqual: return D3D11_COMPARISON_NOT_EQUAL;
		case CF_Never: return D3D11_COMPARISON_NEVER;
		default: return D3D11_COMPARISON_ALWAYS;
		};
	}

	static D3D11_COMPARISON_FUNC TranslateSamplerCompareFunction(ESamplerCompareFunction SamplerComparisonFunction)
	{
		switch (SamplerComparisonFunction)
		{
		case SCF_Less: return D3D11_COMPARISON_LESS;
		case SCF_Never:
		default: return D3D11_COMPARISON_NEVER;
		};
	}

	static D3D11_STENCIL_OP TranslateStencilOp(EStencilOp StencilOp)
	{
		switch (StencilOp)
		{
		case SO_Zero: return D3D11_STENCIL_OP_ZERO;
		case SO_Replace: return D3D11_STENCIL_OP_REPLACE;
		case SO_SaturatedIncrement: return D3D11_STENCIL_OP_INCR_SAT;
		case SO_SaturatedDecrement: return D3D11_STENCIL_OP_DECR_SAT;
		case SO_Invert: return D3D11_STENCIL_OP_INVERT;
		case SO_Increment: return D3D11_STENCIL_OP_INCR;
		case SO_Decrement: return D3D11_STENCIL_OP_DECR;
		default: return D3D11_STENCIL_OP_KEEP;
		};
	}

	static D3D11_BLEND_OP TranslateBlendOp(EBlendOperation BlendOp)
	{
		switch (BlendOp)
		{
		case BO_Subtract: return D3D11_BLEND_OP_SUBTRACT;
		case BO_Min: return D3D11_BLEND_OP_MIN;
		case BO_Max: return D3D11_BLEND_OP_MAX;
		case BO_ReverseSubtract: return D3D11_BLEND_OP_REV_SUBTRACT;
		default: return D3D11_BLEND_OP_ADD;
		};
	}

	static D3D11_BLEND TranslateBlendFactor(EBlendFactor BlendFactor)
	{
		switch (BlendFactor)
		{
		case BF_One: return D3D11_BLEND_ONE;
		case BF_SourceColor: return D3D11_BLEND_SRC_COLOR;
		case BF_InverseSourceColor: return D3D11_BLEND_INV_SRC_COLOR;
		case BF_SourceAlpha: return D3D11_BLEND_SRC_ALPHA;
		case BF_InverseSourceAlpha: return D3D11_BLEND_INV_SRC_ALPHA;
		case BF_DestAlpha: return D3D11_BLEND_DEST_ALPHA;
		case BF_InverseDestAlpha: return D3D11_BLEND_INV_DEST_ALPHA;
		case BF_DestColor: return D3D11_BLEND_DEST_COLOR;
		case BF_InverseDestColor: return D3D11_BLEND_INV_DEST_COLOR;
		case BF_ConstantBlendFactor: return D3D11_BLEND_BLEND_FACTOR;
		case BF_InverseConstantBlendFactor: return D3D11_BLEND_INV_BLEND_FACTOR;
		case BF_Source1Color: return D3D11_BLEND_SRC1_COLOR;
		case BF_InverseSource1Color: return D3D11_BLEND_INV_SRC1_COLOR;
		case BF_Source1Alpha: return D3D11_BLEND_SRC1_ALPHA;
		case BF_InverseSource1Alpha: return D3D11_BLEND_INV_SRC1_ALPHA;

		default: return D3D11_BLEND_ZERO;
		};
	}

	struct D3D11SamplerStateP
	{
		D3D11DynamicRHI* D3D11RHI = nullptr;
		win32::com_ptr<ID3D11SamplerState> SamplerStateHandle;
	};


	D3D11SamplerState::D3D11SamplerState(D3D11DynamicRHI* D3D11RHI)
		:Impl(std::make_shared<D3D11SamplerStateP>())
	{
		Impl->D3D11RHI = D3D11RHI;
	}

	D3D11SamplerState::~D3D11SamplerState()
	{

	}

	bool D3D11SamplerState::CreateSamplerState(const SamplerStateInitializerRHI& Initializer)
	{
		D3D11_SAMPLER_DESC SamplerDesc;
		ZeroMemory(&SamplerDesc, sizeof(D3D11_SAMPLER_DESC));

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
				SamplerDesc.Filter = bComparisonEnabled ? D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			}
			else
			{
				// D3D11 doesn't allow using point filtering for mip filter when using anisotropic filtering
				SamplerDesc.Filter = bComparisonEnabled ? D3D11_FILTER_COMPARISON_ANISOTROPIC : D3D11_FILTER_ANISOTROPIC;
			}

			break;
		case SF_Trilinear:
			SamplerDesc.Filter = bComparisonEnabled ? D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			break;
		case SF_Bilinear:
			SamplerDesc.Filter = bComparisonEnabled ? D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT : D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			break;
		case SF_Point:
			SamplerDesc.Filter = bComparisonEnabled ? D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_POINT;
			break;
		}
		const core::FLinearColor LinearBorderColor = core::FColor(Initializer.BorderColor);
		SamplerDesc.BorderColor[0] = LinearBorderColor.R;
		SamplerDesc.BorderColor[1] = LinearBorderColor.G;
		SamplerDesc.BorderColor[2] = LinearBorderColor.B;
		SamplerDesc.BorderColor[3] = LinearBorderColor.A;
		SamplerDesc.ComparisonFunc = TranslateSamplerCompareFunction(Initializer.SamplerComparisonFunction);

		// D3D11 will return the same pointer if the particular state description was already created
		VERIFYD3D11RESULT(Impl->D3D11RHI->GetDevice()->CreateSamplerState(&SamplerDesc, Impl->SamplerStateHandle.get_init_ref()));
		return Impl->SamplerStateHandle.is_valid();
	}

	ID3D11SamplerState* D3D11SamplerState::GetNativeSampleState() const
	{
		return Impl->SamplerStateHandle.get();
	}

}