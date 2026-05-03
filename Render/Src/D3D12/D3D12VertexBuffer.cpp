#include "D3D12/D3D12VertexBuffer.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12CommandContext.h"
#include <cstring>

namespace RenderCore
{
	struct D3D12VertexBfferPrivate
	{
		FD3D12Resource* Resource = nullptr;
		int32_t StrideByteWidth = 0;
		int32_t Count = 0;
		int32_t BufferSize = 0;
		bool bDynamic = false;

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
		d->bDynamic = (InUsage & BUF_AnyDynamic) != 0;

		D3D12_RESOURCE_DESC ResDesc = DescribeBuffer();

		// Opt-in via BUF_KeepCPUAccessible: UPLOAD + Map avoids InitializeBuffer (submit/fence) during fragile early init.
		// Without it, behavior matches pre-change (DEFAULT + GPU copy) — e.g. glTF uses BUF_Dynamic and is unaffected.
		if (InData && !d->bDynamic && (InUsage & BUF_KeepCPUAccessible))
		{
			D3D12_HEAP_PROPERTIES UploadHeapProps{};
			UploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
			UploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			UploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			UploadHeapProps.CreationNodeMask = 1;
			UploadHeapProps.VisibleNodeMask = 1;
			static int32_t sUploadCounter = 0;
			std::wstring Name = core::formatw("W:", d->BufferSize, "_VertexUpload_", ++sUploadCounter);
			HRESULT hr = GetParentAdapter()->CreateCommittedResource(ResDesc, UploadHeapProps, D3D12_RESOURCE_STATE_GENERIC_READ,
																	 nullptr, &d->Resource, Name.c_str());
			if (FAILED(hr))
				return false;
			void* Mapped = d->Resource->Map(nullptr);
			if (!Mapped)
				return false;
			std::memcpy(Mapped, InData, static_cast<size_t>(d->BufferSize));
			d->Resource->Unmap();
			return true;
		}

		// Vertex buffers for dynamic / no-initial-data: DEFAULT; initial data uses GPU copy via command list.
		D3D12_RESOURCE_STATES InitState = D3D12_RESOURCE_STATE_COMMON;
		D3D12_HEAP_PROPERTIES HeapProps;
		HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
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
			GetParentDevice()->GetDefaultCommandContext()->InitializeBuffer(d->Resource, InData, d->BufferSize, 0);
		}

		return true;
	}

	void D3D12VertexBffer::UpdateVertexBUffer(const void* InData, int32_t nVertex, int32_t sizePerVertex)
	{
		C_P(D3D12VertexBffer);
		if (!d->Resource)
			return;
		if (!InData || nVertex <= 0 || sizePerVertex <= 0)
			return;

		const uint32_t NumBytes = (uint32_t)(nVertex * sizePerVertex);
		if (NumBytes == 0)
			return;
		if (NumBytes > (uint32_t)d->BufferSize)
			return;

		if (d->Resource->GetHeapType() == D3D12_HEAP_TYPE_UPLOAD)
		{
			void* Mapped = d->Resource->Map(nullptr);
			if (Mapped)
			{
				std::memcpy(Mapped, InData, NumBytes);
				d->Resource->Unmap();
			}
			return;
		}

		if (std::shared_ptr<D3D12CommandContext> Ctx = GetParentDevice()->GetDefaultCommandContext())
			Ctx->InitializeBuffer(d->Resource, InData, NumBytes, 0);
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