#pragma once
#include "RHI/RHIDefinitions.h"

namespace RenderCore
{
	class RHIVertexBuffer
	{
	public:
		RHIVertexBuffer() = default;
		virtual ~RHIVertexBuffer() {}

		virtual bool CreateVertexBuffer(const void* Data, EBufferUsageFlags InUsage, int32_t StrideByteWidth, int32_t Count) = 0;
		virtual void UpdateVertexBUffer(const void* InData, int32_t nVertex, int32_t sizePerVertex) = 0;
		virtual int32_t GetStride() const = 0;
		virtual int32_t GetCount() const = 0;
	};
}

