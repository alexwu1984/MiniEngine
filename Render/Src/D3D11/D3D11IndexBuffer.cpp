#include "D3D11/D3D11IndexBuffer.h"
#include "RHI/RHIDefinitions.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11RHI.h"

namespace RenderCore
{
	struct D3D11IndexBufferP
	{
		win32::com_ptr<ID3D11Buffer> Buffer;
		D3D11DynamicRHI* D3D11RHI = nullptr;

		uint32_t IndexCount = 0;
		uint32_t Size = 0;
		int32_t IndexFormat = DXGI_FORMAT_R16_UINT;
	};

	D3D11IndexBuffer::D3D11IndexBuffer(D3D11DynamicRHI* D3D11RHI)
		:Data(std::make_shared<D3D11IndexBufferP>())
	{
		Data->D3D11RHI = D3D11RHI;
	}

	D3D11IndexBuffer::~D3D11IndexBuffer()
	{

	}

	bool D3D11IndexBuffer::CreateIndexBuffer(const uint16_t* InData, int32_t InUsage, int32_t IndexCount)
	{
		Data->IndexFormat = DXGI_FORMAT_R16_UINT;
		Data->IndexCount = IndexCount;
		Data->Size = sizeof(uint16_t) * IndexCount;
		return CreateBuffer(InData,InUsage);
	}

	bool D3D11IndexBuffer::CreateIndexBuffer(const uint32_t* InData, int32_t InUsage, int32_t IndexCount)
	{
		Data->IndexFormat = DXGI_FORMAT_R32_UINT;
		Data->IndexCount = IndexCount;
		Data->Size = sizeof(uint32_t) * IndexCount;
		return CreateBuffer(InData, InUsage);
	}

	bool D3D11IndexBuffer::CreateBuffer(const void* InData, int32_t InUsage)
	{
		// Describe the index buffer.
		D3D11_BUFFER_DESC Desc;
		ZeroMemory(&Desc, sizeof(D3D11_BUFFER_DESC));
		Desc.ByteWidth = Data->Size;
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

		D3D11_SUBRESOURCE_DATA indexInitData;
		memset(&indexInitData, 0, sizeof(D3D11_SUBRESOURCE_DATA));
		indexInitData.pSysMem = InData;
		HRESULT hr = Data->D3D11RHI->GetDevice()->CreateBuffer(&Desc, &indexInitData, Data->Buffer.get_init_ref());
		return SUCCEEDED(hr);
	}

}