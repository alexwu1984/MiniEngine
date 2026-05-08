#include "D3D12/D3D12IndirectArgsBuffer.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12CommandContext.h"
#include <cstring>

namespace RenderCore
{
	struct D3D12IndirectArgsBufferPrivate
	{
		FD3D12Resource* Resource = nullptr;
		uint32_t ByteSize = 0;

		~D3D12IndirectArgsBufferPrivate()
		{
			if (Resource)
				Resource->Release();
		}
	};

	D3D12IndirectArgsBuffer::D3D12IndirectArgsBuffer(std::weak_ptr<FD3D12Adapter> InParent)
		: FD3D12AdapterChild(InParent)
		, d_ptr(new D3D12IndirectArgsBufferPrivate())
	{
	}

	D3D12IndirectArgsBuffer::~D3D12IndirectArgsBuffer()
	{
		delete d_ptr;
	}

	bool D3D12IndirectArgsBuffer::CreateBuffer(uint32_t ByteSize, EBufferUsageFlags InUsage, const void* InitialData)
	{
		C_P(D3D12IndirectArgsBuffer);
		d->ByteSize = ByteSize;
		if (!ByteSize)
			return false;

		D3D12_RESOURCE_DESC ResDesc = DescribeBuffer();
		if (InUsage & BUF_UnorderedAccess)
			ResDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_RESOURCE_STATES InitState = D3D12_RESOURCE_STATE_COMMON;
		D3D12_HEAP_PROPERTIES HeapProps{};
		HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProps.CreationNodeMask = 1;
		HeapProps.VisibleNodeMask = 1;

		static int32_t gCounter = 0;
		std::wstring Name = core::formatw("W:", d->ByteSize, "_IndirectArgs_", ++gCounter);
		HRESULT hr = GetParentAdapter()->CreateCommittedResource(ResDesc, HeapProps, InitState, nullptr, &d->Resource, Name.c_str());
		if (FAILED(hr))
			return false;

		if (InitialData)
			GetParentDevice()->GetDefaultCommandContext()->InitializeBuffer(d->Resource, InitialData, d->ByteSize, 0);

		return true;
	}

	uint32_t D3D12IndirectArgsBuffer::GetByteSize() const
	{
		C_P(const D3D12IndirectArgsBuffer);
		return d->ByteSize;
	}

	void D3D12IndirectArgsBuffer::UpdateContents(const void* Data, uint32_t ByteOffset, uint32_t NumBytes)
	{
		C_P(D3D12IndirectArgsBuffer);
		if (!d->Resource || !Data || !NumBytes)
			return;
		if (NumBytes > d->ByteSize || ByteOffset > d->ByteSize - NumBytes)
			return;

		if (std::shared_ptr<D3D12CommandContext> Ctx = GetParentDevice()->GetDefaultCommandContext())
			Ctx->InitializeBuffer(d->Resource, Data, NumBytes, ByteOffset);
	}

	FD3D12Resource* D3D12IndirectArgsBuffer::GetResource() const
	{
		C_P(const D3D12IndirectArgsBuffer);
		return d->Resource;
	}

	D3D12_RESOURCE_DESC D3D12IndirectArgsBuffer::DescribeBuffer() const
	{
		C_P(const D3D12IndirectArgsBuffer);

		D3D12_RESOURCE_DESC Desc{};
		Desc.Alignment = 0;
		Desc.DepthOrArraySize = 1;
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		Desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		Desc.Format = DXGI_FORMAT_UNKNOWN;
		Desc.Width = (UINT64)d->ByteSize;
		Desc.Height = 1;
		Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		Desc.MipLevels = 1;
		Desc.SampleDesc.Count = 1;
		Desc.SampleDesc.Quality = 0;
		return Desc;
	}

	std::shared_ptr<FD3D12Device> D3D12IndirectArgsBuffer::GetParentDevice() const
	{
		return GetParentAdapter()->GetDevice();
	}
}
