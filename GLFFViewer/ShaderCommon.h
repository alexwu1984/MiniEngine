#pragma once
#include "RHI/RHIShdader.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIShaderDefine.h"
#include "Render/MaterialPreFrame.h"

BEGIN_SHADER_STRUCT(PSRenderDemoContant, 5)
DECLARE_PARAM(float, Exposure)
DECLARE_PARAM(int32_t, MipLevel)
DECLARE_PARAM(int32_t, MaxMipLevel)
DECLARE_PARAM(int32_t, NumSamplesPerDir)
BEGIN_STRUCT_CONSTRUCT(PSRenderDemoContant)
END_STRUCT_CONSTRUCT
END_SHADER_STRUCT