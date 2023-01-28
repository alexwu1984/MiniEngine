#pragma once
#include "RHI/RHIStaticState.h"
#include "RHI/RHIShdader.h"

namespace RenderCore
{

	struct GraphicsPipelineStateInitializer
	{
		GraphicsPipelineStateInitializer() = default;
		~GraphicsPipelineStateInitializer() = default;

		std::shared_ptr<RHIBlendState> BlendState;
		std::shared_ptr<RHIRasterizerState> RasterizerState;
		std::shared_ptr<RHIDepthStencilState> DepthStencilState;
		EPrimitiveType	PrimitiveType = PT_TriangleList;
		std::shared_ptr< RHIVertexShader> VertexShader;
		std::shared_ptr< RHIPixelShader> PixelShader;
	};
}