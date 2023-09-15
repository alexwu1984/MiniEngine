#include "Render/PreProcessor.h"
#include "Render/IBLRender.h"
#include "RHI/DynamicRHI.h"

namespace Engine
{
	struct PreProcessorPrivate
	{
		IBLRender GenIBL;
	};

	PreProcessor::PreProcessor()
		:d_ptr(new PreProcessorPrivate())
	{

	}

	PreProcessor::~PreProcessor()
	{
		delete d_ptr;
	}

	void PreProcessor::InitResource(RenderCore::DynamicRHI* RHI)
	{
		C_P(PreProcessor);
		d->GenIBL.InitResource(RHI);
	}

	void PreProcessor::Draw(RenderCore::RHICommandContext& RHIContext)
	{

	}

}