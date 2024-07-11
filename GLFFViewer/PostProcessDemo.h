#pragma once
#include "Render/SimplePostProcessor.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIShaderDefine.h"

BEGIN_SHADER_STRUCT(cbTransition1, 0)
	DECLARE_PARAM_VALUE(int32_t, endx, 2)
	DECLARE_PARAM_VALUE(int32_t, endy, -1)
	DECLARE_PARAM_VALUE(float, progress, 0.2f)
	DECLARE_PARAM_VALUE(float, pad0, 0.f)
	BEGIN_STRUCT_CONSTRUCT(cbTransition1)
	END_STRUCT_CONSTRUCT
END_SHADER_STRUCT


class PostProcessorDemo : public Engine::SimplePostProcessor
{
public:
	PostProcessorDemo(RenderCore::DynamicRHI* RHI);
	virtual ~PostProcessorDemo();

	void InitResource();
	void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, float DeltaTime) override;

private:
	RenderCore::DynamicRHI* _RHI;
	std::shared_ptr< RenderCore::RHIVertexShader> _VertexShader;
	std::shared_ptr< RenderCore::RHIPixelShader> _PixelShader;
	std::shared_ptr< RenderCore::RHITexture2D>  _Texture1;
	std::shared_ptr< RenderCore::RHITexture2D>  _Texture2;
	std::shared_ptr< RenderCore::RHITilePool> _TilePool;
	DECLARE_SHADER_STRUCT_MEMBER(cbTransition1);
};