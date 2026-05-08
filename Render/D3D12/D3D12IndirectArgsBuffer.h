#pragma once
#include "RHI/RHIIndirectArgsBuffer.h"
#include "RHI/RHIDefinitions.h"
#include "D3D12/D3D12RHICommon.h"

namespace RenderCore
{
	class FD3D12Resource;
	struct D3D12IndirectArgsBufferPrivate;

	class D3D12IndirectArgsBuffer : public RHIIndirectArgsBuffer, public FD3D12AdapterChild
	{
	public:
		explicit D3D12IndirectArgsBuffer(std::weak_ptr<FD3D12Adapter> InParent);
		~D3D12IndirectArgsBuffer() override;

		bool CreateBuffer(uint32_t ByteSize, EBufferUsageFlags InUsage, const void* InitialData);

		uint32_t GetByteSize() const override;
		void UpdateContents(const void* Data, uint32_t ByteOffset, uint32_t NumBytes) override;

		FD3D12Resource* GetResource() const;

	private:
		D3D12_RESOURCE_DESC DescribeBuffer() const;
		std::shared_ptr<FD3D12Device> GetParentDevice() const;

		D3D12IndirectArgsBufferPrivate* d_ptr = nullptr;
	};
}
