#include "D3D12/D3D12VertexBuffer.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12CommandContext.h"

namespace RenderCore
{
	struct D3D12VertexBfferPrivate
	{
		FD3D12Resource* Resource = nullptr;
		int32_t StrideByteWidth = 0;
		int32_t Count = 0;
		int32_t BufferSize = 0;

		~D3D12VertexBfferPrivate()
		{
			if (Resource)
				Resource->Release();
		}
	};

	D3D12VertexBffer::D3D12VertexBffer(std::weak_ptr<FD3D12Adapter> InParent)
		:FD3D12AdapterChild(InParent)
		,d_ptr(new D3D12VertexBfferPrivate())
	{

	}

	D3D12VertexBffer::~D3D12VertexBffer()
	{
		delete d_ptr;
	}

	bool D3D12VertexBffer::CreateVertexBuffer(const void* InData, EBufferUsageFlags InUsage, int32_t StrideByteWidth, int32_t Count)
	{
		C_P(D3D12VertexBffer);
		d->BufferSize = StrideByteWidth * Count;
		d->StrideByteWidth = StrideByteWidth;
		d->Count = Count;

		D3D12_RESOURCE_DESC ResDesc = DescribeBuffer();
		D3D12_RESOURCE_STATES InitState = D3D12_RESOURCE_STATE_COMMON;
		D3D12_HEAP_PROPERTIES HeapProps;
		HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		if (InUsage & BUF_AnyDynamic)
		{
			HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
			InitState = D3D12_RESOURCE_STATE_GENERIC_READ;
		}
		HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProps.CreationNodeMask = 1;
		HeapProps.VisibleNodeMask = 1;
		static int32_t gCounter = 0;
		std::wstring Name = core::formatw("W:", d->BufferSize, "_Vertex_", ++gCounter);
		HRESULT hr = GetParentAdapter()->CreateCommittedResource(ResDesc, HeapProps, InitState,
																 nullptr, &d->Resource, Name.c_str());
		if (FAILED(hr))
			return false;
		if (InData)
		{
			if (InUsage & BUF_AnyDynamic)
			{
				void* MappedData = d->Resource->Map();
				if (MappedData)
					memcpy(MappedData, InData, d->BufferSize);
				d->Resource->Unmap();
			}
			else
				GetParentDevice()->GetDefaultCommandContext()->InitializeBuffer(d->Resource, InData, d->BufferSize, 0);
		}

		return true;
	}

	void D3D12VertexBffer::UpdateVertexBUffer(const void* InData, int32_t nVertex, int32_t sizePerVertex)
	{
		C_P(D3D12VertexBffer);
		if (!d->Resource)
			return;
		void* MappedData = d->Resource->Map();
		if (MappedData)
			memcpy(MappedData, InData, nVertex * sizePerVertex);
		d->Resource->Unmap();
	}

	int32_t D3D12VertexBffer::GetStride() const
	{
		C_P(const D3D12VertexBffer);
		return d->StrideByteWidth;
	}

	int32_t D3D12VertexBffer::GetCount() const
	{
		C_P(const D3D12VertexBffer);
		return d->Count;
	}

	D3D12_VERTEX_BUFFER_VIEW D3D12VertexBffer::VertexBufferView(size_t Offset, uint32_t Size, uint32_t Stride) const
	{
		C_P(D3D12VertexBffer);
		Assert(d->Resource);
		D3D12_VERTEX_BUFFER_VIEW VBView{};
		if (!d->Resource)
			return VBView;
		VBView.BufferLocation = d->Resource->GetGPUVirtualAddress() + Offset;
		VBView.SizeInBytes = Size;
		VBView.StrideInBytes = Stride;
		return VBView;
	}

	D3D12_VERTEX_BUFFER_VIEW D3D12VertexBffer::VertexBufferView(size_t BaseVertexIndex /*= 0*/) const
	{
		C_P(D3D12VertexBffer);
		size_t Offset = BaseVertexIndex * d->StrideByteWidth;
		return VertexBufferView(Offset, (uint32_t)(d->BufferSize - Offset), d->StrideByteWidth);
	}

	D3D12_RESOURCE_DESC D3D12VertexBffer::DescribeBuffer() const
	{
		C_P(const D3D12VertexBffer);

		D3D12_RESOURCE_DESC Desc = {};
		Desc.Alignment = 0;
		Desc.DepthOrArraySize = 1;
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		Desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		Desc.Format = DXGI_FORMAT_UNKNOWN;
		Desc.Width = (UINT64)d->BufferSize;
		Desc.Height = 1;
		Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		Desc.MipLevels = 1;
		Desc.SampleDesc.Count = 1;
		Desc.SampleDesc.Quality = 0;
		return Desc;
	}

	std::shared_ptr<FD3D12Device> D3D12VertexBffer::GetParentDevice() const
	{
		return GetParentAdapter()->GetDevice();
	}

}