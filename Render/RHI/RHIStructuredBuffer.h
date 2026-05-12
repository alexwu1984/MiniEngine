#pragma once
#include "RHI/RHIDefinitions.h"

namespace RenderCore
{
	/**
	 * GPU-typed buffer (HLSL `StructuredBuffer<T>` SRV, optional `RWStructuredBuffer<T>` UAV).
	 * Usage flag combinations:
	 *   - BUF_Static                       : DEFAULT heap, initial CPU upload, SRV-only.
	 *   - BUF_Dynamic                      : UPLOAD heap ring (RHIRecommendedParallelFrameResourceSlots slots);
	 *                                        UpdateStructuredBuffer rotates ring slots, SRV-only.
	 *   - BUF_Static | BUF_UnorderedAccess : DEFAULT heap with ALLOW_UNORDERED_ACCESS, both SRV and UAV;
	 *                                        no CPU update path (cluster CS / GPU writes the contents).
	 *   - BUF_Dynamic | BUF_UnorderedAccess: invalid (UPLOAD heap forbids UAV writes).
	 */
	class RHIStructuredBuffer
	{
	public:
		RHIStructuredBuffer() = default;
		virtual ~RHIStructuredBuffer() {}

		virtual bool CreateStructuredBuffer(uint32_t ElementStride, uint32_t ElementCount, EBufferUsageFlags Usage, const void* InitialData) = 0;
		/** Replace the buffer contents (must be created with BUF_Dynamic). */
		virtual void UpdateStructuredBuffer(const void* Contents, uint32_t SizeInBytes) = 0;

		virtual uint32_t GetElementStride() const = 0;
		virtual uint32_t GetElementCount() const = 0;
		/** True when created with BUF_UnorderedAccess; UAV-only setters require this. */
		virtual bool HasUAV() const { return false; }
		uint32_t GetSizeInBytes() const { return GetElementStride() * GetElementCount(); }
	};
}
