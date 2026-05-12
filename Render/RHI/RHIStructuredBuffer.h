#pragma once
#include "RHI/RHIDefinitions.h"

namespace RenderCore
{
	/**
	 * GPU-readable typed buffer (HLSL `StructuredBuffer<T>`), bound through the SRV table
	 * alongside textures. Dynamic instances allow per-frame Update() before draw; Static
	 * instances are initialized once at creation.
	 *
	 * PR1 plumbing: a single backing resource per buffer (no ring) - the caller must ensure
	 * Update() isn't issued between GPU reads of the same slot in flight. A multi-slot
	 * ring will be layered on once the clustered light pass exercises this path.
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
		uint32_t GetSizeInBytes() const { return GetElementStride() * GetElementCount(); }
	};
}
