#include "PostProcessDemo.h"
#include "core/system.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIViewPort.h"
#include "Engine/Engine.h"
#include "Render/GBuffer.h"
#include "Render/TemporalAA.h"
#include "Render/Bloom.h"
#include "Render/RenderUtil.h"

PostProcessorDemo::PostProcessorDemo(RenderCore::DynamicRHI* RHI)
	:_RHI(RHI)
{

}

PostProcessorDemo::~PostProcessorDemo()
{

}

void PostProcessorDemo::InitResource()
{

}

void PostProcessorDemo::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort)
{

}
