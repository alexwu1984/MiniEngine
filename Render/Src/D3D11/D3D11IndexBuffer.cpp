#include "D3D11/D3D11IndexBuffer.h"
#include "RHI/RHIDefinitions.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11RHI.h"

namespace RenderCore
{
	struct D3D11IndexBufferPrivate
	{
		win32::com_ptr<ID3D11Buffer> Buffer;
		D3D11DynamicRHI* D3D11RHI = nullptr;

		uint32_t IndexCount = 0;
		uint32_t Size = 0;
		int32_t IndexFormat = DXGI_FORMAT_R16_UINT;
	};

	D3D11IndexBuffer::D3D11IndexBuffer(D3D11DynamicRHI* D3D11RHI)
		:d_ptr(new D3D11IndexBufferPrivate())
	{
		C_P(D3D11IndexBuffer);
		d_ptr->D3D11RHI = D3D11RHI;
	}

	D3D11IndexBuffer::~D3D11IndexBuffer()
	{
		delete d_ptr;
	}

	bool D3D11IndexBuffer::CreateIndexBuffer(const uint16_t* InData, int32_t InUsage, int32_t TriangleNumber)
	{
		C_P(D3D11IndexBuffer);
		d->IndexFormat = DXGI_FORMAT_R16_UINT;
		d->IndexCount = TriangleNumber * 3;
		d->Size = sizeof(uint16_t) * d->IndexCount;
		return CreateBuffer(InData,InUsage);
	}

	bool D3D11IndexBuffer::CreateIndexBuffer(const uint32_t* InData, int32_t InUsage, int32_t TriangleNumber)
	{
		C_P(D3D11IndexBuffer);
		d->IndexFormat = DXGI_FORMAT_R32_UINT;
		d->IndexCount = TriangleNumber * 3;
		d->Size = sizeof(uint32_t) * d->IndexCount;
		return CreateBuffer(InData, InUsage);
	}

	ID3D11Buffer* D3D11IndexBuffer::GetNativeBuffer() const
	{
		C_P(D3D11IndexBuffer);
		return d->Buffer.get();
	}

	int32_t D3D11IndexBuffer::GetIndexFormat() const
	{
		C_P(D3D11IndexBuffer);
		return d->IndexFormat;
	}

	int32_t D3D11IndexBuffer::GetIndexCount() const
	{
		C_P(D3D11IndexBuffer);
		return d->IndexCount;
	}

	bool D3D11IndexBuffer::CreateBuffer(const void* InData, int32_t InUsage)
	{
		C_P(D3D11IndexBuffer);
		// Describe the index buffer.
		D3D11_BUFFER_DESC Desc;
		ZeroMemory(&Desc, sizeof(D3D11_BUFFER_DESC));
		Desc.ByteWidth = d->Size;
		Desc.Usage = (InUsage & BUF_AnyDynamic) ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		Desc.CPUAccessFlags = (InUsage & BUF_AnyDynamic) ? D3D11_CPU_ACCESS_WRITE : 0;
		Desc.MiscFlags = 0;

		if (InUsage & BUF_UnorderedAccess)
		{
			Desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
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

		D3D11_SUBRESOURCE_DATA IndexInitData{};
		IndexInitData.pSysMem = InData;
		HRESULT hr = d->D3D11RHI->GetDevice()->CreateBuffer(&Desc, &IndexInitData, d->Buffer.get_init_ref());
		return SUCCEEDED(hr);
	}

}