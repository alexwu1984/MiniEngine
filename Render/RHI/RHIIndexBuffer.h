#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHIIndexBuffer
	{
	public:
		RHIIndexBuffer() = default;
		virtual ~RHIIndexBuffer() {}

		virtual bool CreateIndexBuffer(const uint16_t* InData, int32_t InUsage, int32_t IndexCount) = 0;
		virtual bool CreateIndexBuffer(const uint32_t* InData, int32_t InUsage, int32_t IndexCount) = 0;
	};
}