#pragma once
#include "RHI/RHIUniformBuffer.h"
#include "RHIPrivate/D3D11RHIDeclare.h"

namespace RenderCore
{
	class D3D11DynamicRHI;

	struct D3D11UniformBufferP;

	class D3D11UniformBuffer final : public RHIUniformBuffer
	{
	public:
		D3D11UniformBuffer(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11UniformBuffer() {};

		virtual bool CreateUniformBuffer(const void* Contents, uint32_t ConstantBufferSize) override;
		virtual uint32_t GetConstantBufferSize() const override;
		ID3D11Buffer* GetNativeUniformBuffer() const;

	private:
		std::shared_ptr<D3D11UniformBufferP> Impl;
	};
}