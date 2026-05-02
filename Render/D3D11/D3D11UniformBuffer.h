#pragma once
#include "RHI/RHIUniformBuffer.h"
#include "RHIPrivate/D3D11RHIDeclare.h"

namespace RenderCore
{
	class D3D11DynamicRHI;

	struct D3D11UniformBufferPrivate;

	class D3D11UniformBuffer : public RHIUniformBuffer
	{
	public:
		D3D11UniformBuffer(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11UniformBuffer();

		virtual bool CreateUniformBuffer(const void* Contents, uint32_t ConstantBufferSize) override;
		virtual uint32_t GetConstantBufferSize() const override;
		ID3D11Buffer* GetNativeUniformBuffer() const;

	private:
		D3D11UniformBufferPrivate* d_ptr = nullptr;
	};
}