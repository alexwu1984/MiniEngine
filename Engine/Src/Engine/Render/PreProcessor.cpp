#include "Render/PreProcessor.h"
#include "Render/IBLRender.h"
#include "RHI/DynamicRHI.h"

namespace Engine
{
	struct PreProcessorPrivate
	{
		std::shared_ptr<IBLRender> GenIBL;
		RenderCore::DynamicRHI* RHI;

		PreProcessorPrivate(RenderCore::DynamicRHI* _RHI)
			:RHI(_RHI)
		{
			GenIBL = std::make_shared<IBLRender>(_RHI);
		}
	};

	PreProcessor::PreProcessor(RenderCore::DynamicRHI* RHI)
		:d_ptr(new PreProcessorPrivate(RHI))
	{

	}

	PreProcessor::~PreProcessor()
	{
		delete d_ptr;
	}

	void PreProcessor::InitResource()
	{
		C_P(PreProcessor);
		d->GenIBL->InitResource();
	}

	void PreProcessor::LoadConfig(const std::wstring& FileName)
	{
		C_P(PreProcessor);
		d->GenIBL->LoadConfig(FileName);
	}

	void PreProcessor::Draw(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(PreProcessor);
		d->GenIBL->Draw(RHIContext);
	}

	std::shared_ptr<IBLRender> PreProcessor::GetIBLRender()
	{
		C_P(PreProcessor);
		return d->GenIBL;
	}

}