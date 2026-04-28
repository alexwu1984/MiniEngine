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
		// UE-style: vertex buffers live in DEFAULT memory.
		// Dynamic updates use transient UPLOAD allocations + CopyBufferRegion, not committed UPLOAD buffers.
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
			// Initialize via transient upload + GPU copy (keeps DEFAULT residency and avoids WC commit growth).
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

		// Dynamic update path: stage into transient UPLOAD allocator and copy into DEFAULT VB.
		// This prevents long-lived mapped committed UPLOAD buffers from growing VMemPrivate WC.
		auto Ctx = GetParentDevice()->GetDefaultCommandContext();
		if (!Ctx)
			return;

		D3D12CommandListHandle& CL = Ctx->GetCurrentCommandListHandle();
		if (!CL)
		{
			// If no command list is open yet, fall back to the slower init path.
			// (Should be rare; typical frame flow has an open CL.)
			Ctx->InitializeBuffer(d->Resource, InData, NumBytes, 0);
			return;
		}

		FAllocation UploadAlloc = Ctx->GetLinearAllocator(UploadFastAllocator).Allocate(NumBytes, DEFAULT_ALIGN);
		if (!UploadAlloc.CPU || !UploadAlloc.D3D12Resource)
			return;
		memcpy(UploadAlloc.CPU, InData, NumBytes);

		// Transition VB to COPY_DEST, perform copy, then back to GENERIC_READ for binding.
		Ctx->TransitionResource(d->Resource, D3D12_RESOURCE_STATE_COPY_DEST, true);
		CL->CopyBufferRegion(d->Resource->GetResource(), 0, UploadAlloc.D3D12Resource, (UINT64)UploadAlloc.Offset, NumBytes);
		++Ctx->numCopies;
		Ctx->TransitionResource(d->Resource, D3D12_RESOURCE_STATE_GENERIC_READ, true);
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