#include "Render/PostProcessor.h"

namespace Engine
{
	struct PostProcessorPrivate
	{
		RenderCore::DynamicRHI* RHI = nullptr;
	};

	PostProcessor::PostProcessor(RenderCore::DynamicRHI* RHI)
		:d_ptr(new PostProcessorPrivate())
	{
		C_P(PostProcessor);
		d->RHI = RHI;
	}

	PostProcessor::~PostProcessor()
	{
		delete d_ptr;
	}

	void PostProcessor::InitResource()
	{

	}

	void PostProcessor::Draw(RenderCore::RHICommandContext& RHIContext)
	{

	}

}