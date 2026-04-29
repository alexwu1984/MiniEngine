#pragma once
#include "RHI/RHIUniformBuffer.h"
#include "D3D12/D3D12RHICommon.h"
#include <d3d12.h>

namespace RenderCore
{
	struct D3D12UniformBufferPrivate;
	class D3D12CommandListHandle;

	class D3D12UniformBuffer : public RHIUniformBuffer, public FD3D12AdapterChild
	{
	public:
		D3D12UniformBuffer(std::weak_ptr<FD3D12Adapter> InParentAdapter);
		virtual ~D3D12UniformBuffer();

		virtual bool CreateUniformBuffer(const void* Contents, uint32_t ConstantBufferSize) override;
		virtual uint32_t GetConstantBufferSize() const ;
		void* GetResourceBaseAddress() const;
		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const;
		void UpdateUniformBuffer(const void* Contents);

		/** Active ring slot index for the data currently exposed via VA / CPU pointer (fence-tagged). */
		uint32_t GetActiveRingSlotIndex() const;
		/** Call when this buffer's active ring slot is referenced by the recording command list (before Execute). */
		void RecordGpuReferenceRingSlot(const D3D12CommandListHandle& cmdList);
		/** After Execute on the list that recorded references: stamp fence values for pending ring slots. */
		void OnCmdListSubmitFence(uint64_t fenceValue);
		/** Command list Reset / discard without Execute: drop pending slot bits (GPU never saw them). */
		void CancelPendingGpuFenceTags();
		/** Clear all slot fences (e.g. device idle / ClearState). */
		void ResetGpuRingFences();

	private:
		D3D12UniformBufferPrivate* d_ptr = nullptr;
	};
}