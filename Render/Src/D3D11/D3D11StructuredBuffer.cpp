#include "D3D11/D3D11StructuredBuffer.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11RHI.h"
#include "win/com_ptr.h"
#include "core/inc.h"
#include <cstring>

namespace RenderCore
{
	struct D3D11StructuredBufferPrivate
	{
		D3D11DynamicRHI* D3D11RHI{ nullptr };
		win32::com_ptr<ID3D11Buffer> Buffer;
		win32::com_ptr<ID3D11ShaderResourceView> SRV;
		win32::com_ptr<ID3D11UnorderedAccessView> UAV;
		uint32_t Stride{ 0 };
		uint32_t Count{ 0 };
		uint32_t SizeBytes{ 0 };
		bool bDynamic{ false };
		bool bHasUAV{ false };
	};

	D3D11StructuredBuffer::D3D11StructuredBuffer(D3D11DynamicRHI* InRHI)
		: d_ptr(new D3D11StructuredBufferPrivate())
	{
		C_P(D3D11StructuredBuffer);
		d->D3D11RHI = InRHI;
	}

	D3D11StructuredBuffer::~D3D11StructuredBuffer()
	{
		delete d_ptr;
	}

	bool D3D11StructuredBuffer::CreateStructuredBuffer(uint32_t ElementStride, uint32_t ElementCount, EBufferUsageFlags Usage, const void* InitialData)
	{
		C_P(D3D11StructuredBuffer);
		if (!d->D3D11RHI || ElementStride == 0 || ElementCount == 0)
			return false;

		d->Stride = ElementStride;
		d->Count = ElementCount;
		d->SizeBytes = ElementStride * ElementCount;
		d->bDynamic = (Usage & BUF_AnyDynamic) != 0;
		d->bHasUAV = (Usage & BUF_UnorderedAccess) != 0;
		// D3D11 forbids USAGE_DYNAMIC with UAV bind; fail early so the caller hears about the mistake here.
		if (d->bDynamic && d->bHasUAV)
			return false;

		D3D11_BUFFER_DESC Desc{};
		Desc.ByteWidth = d->SizeBytes;
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (d->bHasUAV ? D3D11_BIND_UNORDERED_ACCESS : 0u);
		Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		Desc.StructureByteStride = ElementStride;
		if (d->bDynamic)
		{
			Desc.Usage = D3D11_USAGE_DYNAMIC;
			Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		}
		else if (d->bHasUAV)
		{
			// UAV buffers must live in DEFAULT (GPU-writable); IMMUTABLE would forbid the bind flag.
			Desc.Usage = D3D11_USAGE_DEFAULT;
			Desc.CPUAccessFlags = 0;
		}
		else
		{
			// IMMUTABLE requires initial data; without it fall back to DEFAULT so UpdateStructuredBuffer via UpdateSubresource still works.
			Desc.Usage = InitialData ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
			Desc.CPUAccessFlags = 0;
		}

		D3D11_SUBRESOURCE_DATA InitData{};
		InitData.pSysMem = InitialData;
		InitData.SysMemPitch = 0;
		InitData.SysMemSlicePitch = 0;

		HRESULT hr = d->D3D11RHI->GetDevice()->CreateBuffer(&Desc, InitialData ? &InitData : nullptr, d->Buffer.get_init_ref());
		if (FAILED(hr) || !d->Buffer.is_valid())
			return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc{};
		SrvDesc.Format = DXGI_FORMAT_UNKNOWN;
		SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		SrvDesc.Buffer.FirstElement = 0;
		SrvDesc.Buffer.NumElements = ElementCount;
		hr = d->D3D11RHI->GetDevice()->CreateShaderResourceView(d->Buffer.get(), &SrvDesc, d->SRV.get_init_ref());
		if (FAILED(hr) || !d->SRV.is_valid())
			return false;

		if (d->bHasUAV)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC UavDesc{};
			UavDesc.Format = DXGI_FORMAT_UNKNOWN;
			UavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			UavDesc.Buffer.FirstElement = 0;
			UavDesc.Buffer.NumElements = ElementCount;
			UavDesc.Buffer.Flags = 0;
			hr = d->D3D11RHI->GetDevice()->CreateUnorderedAccessView(d->Buffer.get(), &UavDesc, d->UAV.get_init_ref());
			if (FAILED(hr) || !d->UAV.is_valid())
				return false;
		}

		return true;
	}

	void D3D11StructuredBuffer::UpdateStructuredBuffer(const void* Contents, uint32_t SizeInBytes)
	{
		C_P(D3D11StructuredBuffer);
		if (!Contents || SizeInBytes == 0 || !d->Buffer.is_valid() || !d->D3D11RHI)
			return;
		const uint32_t NumBytes = SizeInBytes > d->SizeBytes ? d->SizeBytes : SizeInBytes;
		ID3D11DeviceContext* Ctx = d->D3D11RHI->GetDeviceContext();
		if (!Ctx)
			return;

		if (d->bDynamic)
		{
			D3D11_MAPPED_SUBRESOURCE Mapped{};
			HRESULT hr = Ctx->Map(d->Buffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
			if (FAILED(hr) || !Mapped.pData)
				return;
			std::memcpy(Mapped.pData, Contents, NumBytes);
			Ctx->Unmap(d->Buffer.get(), 0);
		}
		else
		{
			Ctx->UpdateSubresource(d->Buffer.get(), 0, nullptr, Contents, 0, 0);
		}
	}

	uint32_t D3D11StructuredBuffer::GetElementStride() const
	{
		C_P(const D3D11StructuredBuffer);
		return d->Stride;
	}

	uint32_t D3D11StructuredBuffer::GetElementCount() const
	{
		C_P(const D3D11StructuredBuffer);
		return d->Count;
	}

	ID3D11ShaderResourceView* D3D11StructuredBuffer::GetSRV() const
	{
		C_P(const D3D11StructuredBuffer);
		return d->SRV.get();
	}

	ID3D11UnorderedAccessView* D3D11StructuredBuffer::GetUAV() const
	{
		C_P(const D3D11StructuredBuffer);
		return d->UAV.get();
	}

	ID3D11Buffer* D3D11StructuredBuffer::GetNativeBuffer() const
	{
		C_P(const D3D11StructuredBuffer);
		return d->Buffer.get();
	}

	bool D3D11StructuredBuffer::HasUAV() const
	{
		C_P(const D3D11StructuredBuffer);
		return d->bHasUAV;
	}
}
