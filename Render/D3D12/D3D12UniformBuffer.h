#pragma once
#include "RHI/RHIUniformBuffer.h"
#include "D3D12/D3D12RHICommon.h"
#include <d3d12.h>

namespace RenderCore
{
	struct D3D12UniformBufferPrivate;

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

	private:
		D3D12UniformBufferPrivate* d_ptr = nullptr;
	};
}