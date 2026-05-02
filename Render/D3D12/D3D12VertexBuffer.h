#pragma once
#include "RHI/RHIVertexBuffer.h"
#include "D3D12/D3D12RHICommon.h"

namespace RenderCore
{
	struct D3D12VertexBfferPrivate;

	class D3D12VertexBffer : public RHIVertexBuffer, public FD3D12AdapterChild
	{
	public:
		D3D12VertexBffer(std::weak_ptr<FD3D12Adapter> InParent);
		virtual ~D3D12VertexBffer();

		virtual bool CreateVertexBuffer(const void* InData, EBufferUsageFlags InUsage, int32_t StrideByteWidth, int32_t Count) override;
		virtual void UpdateVertexBUffer(const void* InData, int32_t nVertex, int32_t sizePerVertex) override;
		virtual int32_t GetStride() const override;
		virtual int32_t GetCount() const override;

		D3D12_VERTEX_BUFFER_VIEW VertexBufferView(size_t Offset, uint32_t Size, uint32_t Stride) const;
		D3D12_VERTEX_BUFFER_VIEW VertexBufferView(size_t BaseVertexIndex = 0) const;

	private:
		D3D12_RESOURCE_DESC DescribeBuffer() const;
		std::shared_ptr<FD3D12Device> GetParentDevice() const;
	private:
		D3D12VertexBfferPrivate* d_ptr = nullptr;
	};
}