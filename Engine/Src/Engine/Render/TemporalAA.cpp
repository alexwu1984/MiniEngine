#include "Render/TemporalAA.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"

namespace Engine
{
	struct TemporallAAPrivate
	{
		RenderCore::DynamicRHI* RHI;
	};

	TemporallAA::TemporallAA(RenderCore::DynamicRHI* RHI)
		:d_ptr(new TemporallAAPrivate())
	{
		C_P(TemporallAA);
		d->RHI = RHI;
	}

	TemporallAA::~TemporallAA()
	{
		delete d_ptr;
	}

	void TemporallAA::InitResource()
	{

	}

}