#pragma once
#include "RHI/RHIDefinitions.h"

namespace RenderCore
{
	class RHIUniformBuffer
	{
	public:
		RHIUniformBuffer() = default;
		virtual ~RHIUniformBuffer() {};
		virtual bool CreateUniformBuffer(const void* Contents, uint32_t ConstantBufferSize) = 0;
		virtual uint32_t GetConstantBufferSize() const = 0;
	};
}