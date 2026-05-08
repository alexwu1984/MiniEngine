#pragma once
#include <cstdint>

namespace RenderCore
{
	/** GPU buffer holding one or more indirect argument records (packed tightly). Use BUF_DrawIndirect (+ optional BUF_UnorderedAccess) when creating. */
	class RHIIndirectArgsBuffer
	{
	public:
		RHIIndirectArgsBuffer() = default;
		virtual ~RHIIndirectArgsBuffer() = default;

		virtual uint32_t GetByteSize() const = 0;
		virtual void UpdateContents(const void* Data, uint32_t ByteOffset, uint32_t NumBytes) = 0;
	};
}
