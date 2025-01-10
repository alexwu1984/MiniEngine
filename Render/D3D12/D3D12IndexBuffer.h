#pragma once
#include "RHI/RHIIndexBuffer.h"
#include "D3D12/D3D12RHICommon.h"

namespace RenderCore
{
	struct D3D12IndexBufferPrivate;

	class D3D12IndexBuffer : public RHIIndexBuffer, public FD3D12AdapterChild
	{
	public:
		D3D12IndexBuffer(std::weak_ptr<FD3D12Adapter> InParent);
		~D3D12IndexBuffer();

		virtual bool CreateIndexBuffer(const uint16_t* InData, int32_t InUsage, int32_t TriangleNumber) override;
		virtual bool CreateIndexBuffer(const uint32_t* InData, int32_t InUsage, int32_t TriangleNumber) override;
		virtual int32_t GetIndexFormat() const override;
		virtual int32_t GetIndexCount() const override;

		D3D12_INDEX_BUFFER_VIEW IndexBufferView(size_t Offset, uint32_t Size, bool b32Bit = false) const;
		D3D12_INDEX_BUFFER_VIEW IndexBufferView(size_t StartIndex = 0) const;

	private:
		bool CreateBuffer(const void* InData, int32_t InUsage);
		D3D12_RESOURCE_DESC DescribeBuffer() const;
		std::shared_ptr<FD3D12Device> GetParentDevice() const;
	private:
		D3D12IndexBufferPrivate* d_ptr = nullptr;
	};
}