#pragma once
#include "core/inc.h"

namespace RenderCore
{
#define WITH_SLI 0	// Implicit SLI
#define WITH_MGPU 0	// Explicit MGPU
#define MAX_NUM_GPUS 4
	extern  uint32_t GNumExplicitGPUsForRendering;
	extern  uint32_t GNumAlternateFrameRenderingGroups;
}