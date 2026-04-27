#include "D3D12/D3D12IndexBuffer.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12CommandContext.h"

namespace RenderCore
{
	struct D3D12IndexBufferPrivate
	{
		FD3D12Resource* Resource = nullptr;
		uint32_t IndexCount = 0;
		uint32_t Size = 0;
		uint32_t ElementSize = 0;
		int32_t IndexFormat = DXGI_FORMAT_R16_UINT;
		bool bDynamic = false;

		~D3D12IndexBufferPrivate()
		{
			if (Resource)
				Resource->Release();
		}
	};

	D3D12IndexBuffer::D3D12IndexBuffer(std::weak_ptr<FD3D12Adapter> InParent)
		:FD3D12AdapterChild(InParent)
		, d_ptr(new D3D12IndexBufferPrivate())
	{

	}

	D3D12IndexBuffer::~D3D12IndexBuffer()
	{
		delete d_ptr;
	}

	bool D3D12IndexBuffer::CreateIndexBuffer(const uint16_t* InData, int32_t InUsage, int32_t TriangleNumber)
	{
		C_P(D3D12IndexBuffer);
		d->ElementSize = sizeof(uint16_t);
		d->IndexFormat = DXGI_FORMAT_R16_UINT;
		d->IndexCount = TriangleNumber * 3;
		d->Size = sizeof(uint16_t) * d->IndexCount;
		return CreateBuffer(InData, InUsage);
	}

	bool D3D12IndexBuffer::CreateIndexBuffer(const uint32_t* InData, int32_t InUsage, int32_t TriangleNumber)
	{
		C_P(D3D12IndexBuffer);
		d->ElementSize = sizeof(uint32_t);
		d->IndexFormat = DXGI_FORMAT_R32_UINT;
		d->IndexCount = TriangleNumber * 3;
		d->Size = sizeof(uint32_t) * d->IndexCount;
		return CreateBuffer(InData, InUsage);
	}

	int32_t D3D12IndexBuffer::GetIndexFormat() const
	{
		C_P(D3D12IndexBuffer);
		return d->IndexFormat;
	}

	int32_t D3D12IndexBuffer::GetIndexCount() const
	{
		C_P(D3D12IndexBuffer);
		return d->IndexCount;
	}

	D3D12_INDEX_BUFFER_VIEW D3D12IndexBuffer::IndexBufferView(size_t Offset, uint32_t Size, bool b32Bit /*= false*/) const
	{
		C_P(D3D12IndexBuffer);
		D3D12_INDEX_BUFFER_VIEW IBView{};
		if (!d->Resource)
			return IBView;
		IBView.BufferLocation = d->Resource->GetGPUVirtualAddress() + Offset;
		IBView.Format = b32Bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
		IBView.SizeInBytes = Size;
		return IBView;
	}

	D3D12_INDEX_BUFFER_VIEW D3D12IndexBuffer::IndexBufferView(size_t StartIndex /*= 0*/) const
	{
		C_P(const D3D12IndexBuffer);
		size_t Offset = StartIndex * d->ElementSize;
		return IndexBufferView(Offset, (uint32_t)(d->Size - Offset), d->ElementSize == 4);
	}

	bool D3D12IndexBuffer::CreateBuffer(const void* InData, int32_t InUsage)
	{
		C_P(D3D12IndexBuffer);
		D3D12_RESOURCE_DESC ResDesc = DescribeBuffer();

		d->bDynamic = (InUsage & BUF_AnyDynamic) != 0;

		// UE-style: index buffers live in DEFAULT memory.
		// Dynamic updates use transient UPLOAD allocations + CopyBufferRegion, not committed UPLOAD buffers.
		D3D12_RESOURCE_STATES InitState = D3D12_RESOURCE_STATE_GENERIC_READ;
		D3D12_HEAP_PROPERTIES HeapProps;
		HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		
		HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProps.CreationNodeMask = 1;
		HeapProps.VisibleNodeMask = 1;
		static int32_t gCounter = 0;
		std::wstring Name = core::formatw("W:", d->Size, "_Index_", ++gCounter);
		HRESULT hr = GetParentAdapter()->CreateCommittedResource(ResDesc, HeapProps, InitState,
																nullptr, &d->Resource, Name.c_str());
		if (FAILED(hr))
			return false;
		if (InData)
		{
			// Initialize via transient upload + GPU copy (keeps DEFAULT residency and avoids WC commit growth).
			GetParentDevice()->GetDefaultCommandContext()->InitializeBuffer(d->Resource, InData, d->Size, 0);
		}
		return true;
	}

	D3D12_RESOURCE_DESC D3D12IndexBuffer::DescribeBuffer() const
	{
		C_P(const D3D12IndexBuffer);

		D3D12_RESOURCE_DESC Desc = {};
		Desc.Alignment = 0;
		Desc.DepthOrArraySize = 1;
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		Desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		Desc.Format = DXGI_FORMAT_UNKNOWN;
		Desc.Width = (UINT64)d->Size;
		Desc.Height = 1;
		Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		Desc.MipLevels = 1;
		Desc.SampleDesc.Count = 1;
		Desc.SampleDesc.Quality = 0;
		return Desc;
	}

	std::shared_ptr<FD3D12Device> D3D12IndexBuffer::GetParentDevice() const
	{
		return GetParentAdapter()->GetDevice();
	}

}