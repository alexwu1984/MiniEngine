#pragma once
#include "Render/SimplePostProcessor.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIShaderDefine.h"

struct cbTransition1
{
	int32_t endx{ 2 };
	int32_t endy{ -1 };
	float progress{ 0.2f };
	float pad0{ 0.f };
};
using cbTransition1Wrap = RenderCore::TUniformBufferBinding<cbTransition1, 0u>;

class PostProcessorDemo : public Engine::SimplePostProcessor
{
public:
	PostProcessorDemo(RenderCore::DynamicRHI* RHI);
	~PostProcessorDemo() override;

	void InitResource();
	void Draw(RenderCore::RHICommandContext& RHIContext,
			  std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
			  float DeltaTime) override;

private:
	RenderCore::DynamicRHI* RHI = nullptr;
	std::shared_ptr<RenderCore::RHIVertexShader> VertexShader;
	std::shared_ptr<RenderCore::RHIPixelShader> PixelShader;
	std::shared_ptr<RenderCore::RHITexture2D> Texture1;
	std::shared_ptr<RenderCore::RHITexture2D> Texture2;
	DECLARE_SHADER_STRUCT_MEMBER(cbTransition1);
};
