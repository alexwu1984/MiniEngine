#pragma once
#include "Render/SimplePostProcessor.h"

class PostProcessorDemo : public Engine::SimplePostProcessor
{
public:
	PostProcessorDemo(RenderCore::DynamicRHI* RHI);
	virtual ~PostProcessorDemo();

	void InitResource();
	void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort) override;

private:
	RenderCore::DynamicRHI* _RHI;
};