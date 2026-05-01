#include "D3D12/D3D12GenerateMips.h"
#include "D3D12/D3D12RHI.h"
#include "math/matrix4x4.h"
#include "core/system.h"
#include "D3D12/D3D12Adapter.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12ReourceTraits.h"
#include "RHI/RHICachedStates.h"

namespace RenderCore
{
	struct PSContant
	{
		int32_t MipIndex{};
		int32_t NumMips{};
		int32_t CubeFace{};
	};
	using PSContantWrap = TUniformBufferBinding<PSContant, 0u>;

	struct FD3D12GenerateMipsPrivate
	{
		FD3D12GenerateMipsPrivate(std::shared_ptr<D3D12DynamicRHI> InRHI)
			:GET_SHADER_STRUCT_MEMBER(PSContant)(InRHI.get())
			,RHI(InRHI)
		{
		
		}
		DECLARE_SHADER_STRUCT_MEMBER(PSContant);
		std::shared_ptr<RHIVertexShader> CubeVS;
		std::shared_ptr<RHIPixelShader> CubePS;
		std::shared_ptr<D3D12DynamicRHI> RHI;
	};

	FD3D12GenerateMips::FD3D12GenerateMips(std::weak_ptr<FD3D12Adapter> InParent)
		:FD3D12AdapterChild(InParent)
	{
		d_ptr = new FD3D12GenerateMipsPrivate(InParent.lock()->GetOwningRHI());
	}

	FD3D12GenerateMips::~FD3D12GenerateMips()
	{
		delete d_ptr;
	}

	void FD3D12GenerateMips::InitResource()
	{
		C_P(FD3D12GenerateMips);

		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		ShaderPath += L"GenerateMips.hlsl";

		d->CubeVS = d->RHI->RHICreateVertexShader(ShaderPath, "VS_Main_Cube", {}, {});
		d->CubePS = d->RHI->RHICreatePixelShader(ShaderPath, "PS_Main_Cube", {});
	}

	void FD3D12GenerateMips::GenerateForCube(std::shared_ptr<RHITextureCube> TextureCubeRHI, D3D12CommandContext* CommandContext)
	{
		D3D12TextureCube* TextureCube = RHIResourceCast(TextureCubeRHI.get());
		Assert(TextureCube->GetSize().cx == TextureCube->GetSize().cy);
		C_P(FD3D12GenerateMips);
		d->GET_UNIFORMDATA(PSContant).NumMips = TextureCube->GetNumMips();
		uint32_t SrcSize = TextureCube->GetSize().cx;
		for (uint32_t MipLevel = 1; MipLevel < TextureCube->GetNumMips(); ++MipLevel)
		{
			uint32_t DstSize = SrcSize >> MipLevel;
			d->GET_UNIFORMDATA(PSContant).MipIndex = MipLevel;

			for (int F = 0; F < 6; ++F)
			{
				uint32_t SrcSubIndex = TextureCube->GetSubresourceIndex(F, MipLevel - 1);
				CommandContext->TransitionSubResource(TextureCube->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, SrcSubIndex, F == 5);
			}

			GraphicsPipelineStateInitializer Init;
			Init.VertexShader = d->CubeVS;
			Init.PixelShader = d->CubePS;
			Init.BlendState = RHICachedStates::BlendOnAlphaOff;
			Init.DepthStencilState = RHICachedStates::DepthStateDisable;
			Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;
			CommandContext->RHISetGraphicsPipelineState(Init);
			CommandContext->RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::ClampLinerSampler);

			for (int Face = 0; Face < 6; ++Face)
			{
				CommandContext->SetRenderTarget(TextureCube, Face, MipLevel);
				CommandContext->Clear(TextureCubeRHI, Face, MipLevel, core::FLinearColor::Black);
				CommandContext->SetViewPort(0, 0, DstSize, DstSize);

				d->GET_UNIFORMDATA(PSContant).CubeFace = Face;
				RHI_UpdateAndBindUniformBuffer(*CommandContext, d->GET_SHADER_STRUCT_MEMBER(PSContant), SF_Pixel);
				CommandContext->RHISetShaderTexture(RenderCore::SF_Pixel, 0, MipLevel - 1, TextureCubeRHI);
				CommandContext->Draw(6);

				if (MipLevel == TextureCube->GetNumMips() - 1)
				{
					uint32_t DstSubIndex = TextureCube->GetSubresourceIndex(Face, MipLevel);
					CommandContext->TransitionSubResource(TextureCube->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, DstSubIndex, true);
				}
			}

			// One submit per mip (6 faces): fewer dynamic-descriptor heap ClearCache / fence churn than per-face Flush(true).
			CommandContext->FlushCommands(false);
		}
	}

}