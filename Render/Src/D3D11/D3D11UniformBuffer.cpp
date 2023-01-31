#include "D3D11/D3D11UniformBuffer.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11RHI.h"

namespace RenderCore
{
	struct D3D11UniformBufferP
	{
		D3D11DynamicRHI* D3D11RHI{ nullptr };
		win32::com_ptr<ID3D11Buffer> UniformBuffer;
		uint32_t ConstantBufferSize{};
	};

	D3D11UniformBuffer::D3D11UniformBuffer(D3D11DynamicRHI* D3D11RHI)
		:Impl(std::make_shared<D3D11UniformBufferP>())
	{
		Impl->D3D11RHI = D3D11RHI;
	}

	bool D3D11UniformBuffer::CreateUniformBuffer(const void* Contents, uint32_t ConstantBufferSize)
	{
		Assert(win32::Align(ConstantBufferSize, 16) == ConstantBufferSize);

		D3D11_BUFFER_DESC Desc{};
		Desc.ByteWidth = ConstantBufferSize;
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Desc.MiscFlags = 0;
		Desc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA ImmutableData{};
		ImmutableData.pSysMem = Contents;
		ImmutableData.SysMemPitch = ImmutableData.SysMemSlicePitch = 0;

		Impl->ConstantBufferSize = ConstantBufferSize;

		VERIFYD3D11RESULT(Impl->D3D11RHI->GetDevice()->CreateBuffer(&Desc, Contents ? &ImmutableData : nullptr, Impl->UniformBuffer.get_init_ref()));
		return Impl->UniformBuffer.is_valid();
	}

	uint32_t D3D11UniformBuffer::GetConstantBufferSize() const
	{
		return Impl->ConstantBufferSize;
	}

	ID3D11Buffer* D3D11UniformBuffer::GetNativeUniformBuffer() const
	{
		return Impl->UniformBuffer.get();
	}

}