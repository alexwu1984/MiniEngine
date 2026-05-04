#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHIVertexShader;
	class RHIPixelShader;
	class RHITexture2D;
}

namespace Engine
{

	class RenderUtil
	{
	public:
		static void RenderFullQuad(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> Tex,
									std::shared_ptr<RenderCore::RHIVertexShader> VertexShader,
									std::shared_ptr<RenderCore::RHIPixelShader> PixelShader);
	};
}