#include "Render/PreProcessor.h"
#include "Render/SkyLightEnvironment.h"
#include "RHI/DynamicRHI.h"

namespace Engine
{
	struct PreProcessorPrivate
	{
		std::shared_ptr<FSkyLightIBLPrecompute> GenIBL;
		RenderCore::DynamicRHI* RHI;

		PreProcessorPrivate(RenderCore::DynamicRHI* _RHI)
			:RHI(_RHI)
		{
			GenIBL = std::make_shared<FSkyLightIBLPrecompute>(_RHI);
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

	void PreProcessor::LoadConfig(const nlohmann::json& Root)
	{
		C_P(PreProcessor);
		d->GenIBL->LoadConfig(Root);
	}

	void PreProcessor::Draw(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(PreProcessor);
		d->GenIBL->Draw(RHIContext);
	}

	std::shared_ptr<FSkyLightIBLPrecompute> PreProcessor::GetSkyLightEnvironment()
	{
		C_P(PreProcessor);
		return d->GenIBL;
	}

	void PreProcessor::ResolveSkyLightForFrame(std::optional<std::wstring> componentOverrideFullPath)
	{
		C_P(PreProcessor);
		d->GenIBL->ResolveAndApplyHDRSource(std::move(componentOverrideFullPath));
	}

}