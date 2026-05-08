#include "D3D11/D3D11IndirectArgsBuffer.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11RHI.h"
#include "RHI/RHIDefinitions.h"
#include <cstring>

namespace RenderCore
{
	struct D3D11IndirectArgsBuffer::Private
	{
		uint32_t ByteSize = 0;
		bool bDynamic = false;
		win32::com_ptr<ID3D11Buffer> Buffer;
		D3D11DynamicRHI* RHI = nullptr;
	};

	D3D11IndirectArgsBuffer::D3D11IndirectArgsBuffer(D3D11DynamicRHI* InRHI)
		: d_ptr(new Private{})
	{
		d_ptr->RHI = InRHI;
	}

	D3D11IndirectArgsBuffer::~D3D11IndirectArgsBuffer()
	{
		delete d_ptr;
	}

	bool D3D11IndirectArgsBuffer::CreateBuffer(uint32_t ByteSize, EBufferUsageFlags InUsage, const void* InitialData)
	{
		if (!d_ptr->RHI || !ByteSize)
			return false;

		d_ptr->ByteSize = ByteSize;
		d_ptr->bDynamic = (InUsage & BUF_AnyDynamic) != 0;

		D3D11_BUFFER_DESC Desc{};
		Desc.ByteWidth = ByteSize;
		Desc.Usage = (InUsage & BUF_AnyDynamic) ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		Desc.BindFlags = 0;
		if (InUsage & BUF_UnorderedAccess)
			Desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		if (InUsage & BUF_ShaderResource)
			Desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

		Desc.CPUAccessFlags = (InUsage & BUF_AnyDynamic) ? D3D11_CPU_ACCESS_WRITE : 0;
		Desc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

		D3D11_SUBRESOURCE_DATA Init{};
		const D3D11_SUBRESOURCE_DATA* pInit = nullptr;
		if (InitialData)
		{
			Init.pSysMem = InitialData;
			pInit = &Init;
		}

		HRESULT hr = d_ptr->RHI->GetDevice()->CreateBuffer(&Desc, pInit, d_ptr->Buffer.get_init_ref());
		return SUCCEEDED(hr);
	}

	uint32_t D3D11IndirectArgsBuffer::GetByteSize() const
	{
		return d_ptr->ByteSize;
	}

	void D3D11IndirectArgsBuffer::UpdateContents(const void* Data, uint32_t ByteOffset, uint32_t NumBytes)
	{
		if (!d_ptr->RHI || !d_ptr->Buffer || !Data || !NumBytes)
			return;
		if (NumBytes > d_ptr->ByteSize || ByteOffset > d_ptr->ByteSize - NumBytes)
			return;

		ID3D11DeviceContext* Ctx = d_ptr->RHI->GetDeviceContext();
		if (d_ptr->bDynamic)
		{
			D3D11_MAPPED_SUBRESOURCE Mapped{};
			const D3D11_MAP MapType =
				(ByteOffset == 0u && NumBytes == d_ptr->ByteSize) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
			const HRESULT hr = Ctx->Map(d_ptr->Buffer.get(), 0, MapType, 0, &Mapped);
			if (FAILED(hr) || !Mapped.pData)
				return;
			std::memcpy(static_cast<uint8_t*>(Mapped.pData) + ByteOffset, Data, NumBytes);
			Ctx->Unmap(d_ptr->Buffer.get(), 0);
			return;
		}

		D3D11_BOX Dst{};
		Dst.left = ByteOffset;
		Dst.right = ByteOffset + NumBytes;
		Dst.top = 0;
		Dst.bottom = 1;
		Dst.front = 0;
		Dst.back = 1;
		Ctx->UpdateSubresource(d_ptr->Buffer.get(), 0, &Dst, Data, NumBytes, NumBytes);
	}

	ID3D11Buffer* D3D11IndirectArgsBuffer::GetNativeBuffer() const
	{
		return d_ptr->Buffer.get();
	}
}
