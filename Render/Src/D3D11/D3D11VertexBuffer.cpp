#include "D3D11/D3D11VertexBuffer.h"
#include "RHI/RHIDefinitions.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11RHI.h"

namespace RenderCore
{
	struct D3D11VertexBufferPrivate
	{
		int32_t Stride = 0;
		int32_t Count = 0;
		win32::com_ptr<ID3D11Buffer> Buffer;
		D3D11DynamicRHI* D3D11RHI = nullptr;
	};

	D3D11VertexBuffer::D3D11VertexBuffer(D3D11DynamicRHI* D3D11RHI)
		:d_ptr(new D3D11VertexBufferPrivate())
	{
		C_P(D3D11VertexBuffer);
		d->D3D11RHI = D3D11RHI;
	}

	D3D11VertexBuffer::~D3D11VertexBuffer()
	{
		delete d_ptr;
	}

	bool D3D11VertexBuffer::CreateVertexBuffer(const void* InData, EBufferUsageFlags InUsage, int32_t StrideByteWidth, int32_t Count)
	{
		C_P(D3D11VertexBuffer);
		if (!d->D3D11RHI)
			return false;
		d->Stride = StrideByteWidth;
		d->Count = Count;

		D3D11_BUFFER_DESC Desc;
		ZeroMemory(&Desc, sizeof(D3D11_BUFFER_DESC));
		Desc.ByteWidth = StrideByteWidth * Count;
		Desc.Usage = (InUsage & BUF_AnyDynamic) ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		Desc.CPUAccessFlags = (InUsage & BUF_AnyDynamic) ? D3D11_CPU_ACCESS_WRITE : 0;
		//Desc.MiscFlags = 0;
		//Desc.StructureByteStride = 0;

		if (InUsage & BUF_UnorderedAccess)
		{
			Desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}

		if (InUsage & BUF_ByteAddressBuffer)
		{
			Desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		}

		if (InUsage & BUF_StreamOutput)
		{
			Desc.BindFlags |= D3D11_BIND_STREAM_OUTPUT;
		}

		if (InUsage & BUF_DrawIndirect)
		{
			Desc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		}

		if (InUsage & BUF_ShaderResource)
		{
			Desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		}

		if (InUsage & BUF_Shared)
		{
			Desc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
		}

		D3D11_SUBRESOURCE_DATA vertexInitData{};
		//memset(&vertexInitData, 0, sizeof(D3D11_SUBRESOURCE_DATA));
		vertexInitData.pSysMem = InData;
		HRESULT hr = d->D3D11RHI->GetDevice()->CreateBuffer(&Desc, &vertexInitData, d->Buffer.get_init_ref());
		return SUCCEEDED(hr);
	}

	void D3D11VertexBuffer::UpdateVertexBUffer(const void* InData, int32_t nVertex, int32_t sizePerVertex)
	{
		C_P(D3D11VertexBuffer);
		D3D11_MAPPED_SUBRESOURCE mapSubResource;
		HRESULT hr = d->D3D11RHI->GetDeviceContext()->Map(d->Buffer.get(), 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapSubResource);
		if (FAILED(hr))
		{
			return;
		}
		memcpy((uint8_t*)mapSubResource.pData + sizePerVertex, InData, nVertex * sizePerVertex);
		d->D3D11RHI->GetDeviceContext()->Unmap(d->Buffer.get(), 0);
	}

	int32_t D3D11VertexBuffer::GetStride() const
	{
		C_P(D3D11VertexBuffer);
		return d->Stride;
	}

	int32_t D3D11VertexBuffer::GetCount() const
	{
		C_P(D3D11VertexBuffer);
		return d->Count;
	}

	ID3D11Buffer* D3D11VertexBuffer::GetNativeBuffer() const
	{
		C_P(D3D11VertexBuffer);
		return d->Buffer.get();
	}

}