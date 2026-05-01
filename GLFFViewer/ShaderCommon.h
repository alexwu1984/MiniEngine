#pragma once
#include "RHI/RHIShdader.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIShaderDefine.h"
#include "Render/MaterialPreFrame.h"

struct PSRenderDemoContant
{
	float Exposure{};
	int32_t MipLevel{};
	int32_t MaxMipLevel{};
	int32_t NumSamplesPerDir{};
};
using PSRenderDemoContantWrap = RenderCore::TUniformBufferBinding<PSRenderDemoContant, 5u>;
