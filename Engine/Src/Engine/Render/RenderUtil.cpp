#include "Render/RenderUtil.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "Render/RDGUtils.h"

namespace Engine
{

	void RenderUtil::RenderFullQuad(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> Tex, std::shared_ptr<RenderCore::RHIVertexShader> VertexShader, std::shared_ptr<RenderCore::RHIPixelShader> PixelShader)
	{
		RenderCore::GraphicsPipelineStateInitializer Init;
		Init.VertexShader = VertexShader;
		Init.PixelShader = PixelShader;

		Init.BlendState = RenderCore::RHICachedStates::BlendTraditional;
		Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		if (Tex)
			FRDGUtils::RHICmdListDeclarePixelSamplingSrvs(RHIContext, { Tex });
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, Tex);
		RHIContext.Draw(3);
	}

}