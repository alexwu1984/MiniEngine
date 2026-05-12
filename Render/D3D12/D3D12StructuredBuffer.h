#pragma once
#include "RHI/RHIStructuredBuffer.h"
#include "D3D12/D3D12RHICommon.h"
#include <d3d12.h>

namespace RenderCore
{
	struct D3D12StructuredBufferPrivate;
	class FD3D12Device;
	class FD3D12Resource;

	/**
	 * D3D12 implementation of `RHIStructuredBuffer`. Backing model:
	 *   - BUF_Static  : committed DEFAULT-heap resource, initial upload via the default command context.
	 *   - BUF_Dynamic : committed UPLOAD-heap resource sized for `kDynamicRingSlots` back-to-back copies of
	 *                   the user payload, with one offline SRV per slot. UpdateStructuredBuffer advances the
	 *                   slot, memcpys into that slot, and points GetSRV() at the matching descriptor. Slot
	 *                   count matches RHIRecommendedParallelFrameResourceSlots so a Update->Draw pair issued
	 *                   on frame N never overwrites the region the GPU is still reading from frames N-1/N-2.
	 *   - BUF_Static | BUF_UnorderedAccess : DEFAULT-heap with ALLOW_UNORDERED_ACCESS; one offline SRV + one
	 *                   offline UAV descriptor. Used for GPU-produced data (cluster light tables) that is
	 *                   written by a compute pass and read by subsequent draws.
	 * The SRV is created at construction (offline descriptors) and consumed by FD3D12StateCache through the
	 * existing per-frequency SRV cache, so structured buffers share register space with texture SRVs.
	 */
	class D3D12StructuredBuffer : public RHIStructuredBuffer, public FD3D12AdapterChild
	{
	public:
		explicit D3D12StructuredBuffer(std::weak_ptr<FD3D12Adapter> InParentAdapter);
		virtual ~D3D12StructuredBuffer();

		virtual bool CreateStructuredBuffer(uint32_t ElementStride, uint32_t ElementCount, EBufferUsageFlags Usage, const void* InitialData) override;
		virtual void UpdateStructuredBuffer(const void* Contents, uint32_t SizeInBytes) override;

		virtual uint32_t GetElementStride() const override;
		virtual uint32_t GetElementCount() const override;
		virtual bool HasUAV() const override;

		D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const;
		/** Valid only when created with BUF_UnorderedAccess; D3D12_CPU_DESCRIPTOR_HANDLE_NULL otherwise. */
		D3D12_CPU_DESCRIPTOR_HANDLE GetUAV() const;
		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const;
		FD3D12Resource* GetResource() const;
		bool IsDynamic() const;

	private:
		std::shared_ptr<FD3D12Device> GetParentDevice() const;

		D3D12StructuredBufferPrivate* d_ptr = nullptr;
	};
}
