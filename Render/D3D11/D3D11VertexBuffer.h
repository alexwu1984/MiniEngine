#pragma once
#include "RHI/RHIVertexBuffer.h"

namespace RenderCore
{
	struct D3D11VertexBufferP;
	class D3D11DynamicRHI;

	class D3D11VertexBuffer : public RHIVertexBuffer
	{
	public:
		D3D11VertexBuffer(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11VertexBuffer();

		virtual bool CreateVertexBuffer(const void* InData, EBufferUsageFlags InUsage, int32_t StrideByteWidth, int32_t Count) override;
		virtual void UpdateVertexBUffer(const void* InData, int32_t nVertex, int32_t sizePerVertex) override;
		virtual int32_t GetStride() const override;
		virtual int32_t GetCount() const override;

	private:
		std::shared_ptr< D3D11VertexBufferP> Impl;
	};
}
