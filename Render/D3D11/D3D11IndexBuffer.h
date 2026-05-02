#pragma once
#include "RHI/RHIIndexBuffer.h"
#include "RHIPrivate/D3D11RHIDeclare.h"

namespace RenderCore 
{
	struct D3D11IndexBufferPrivate;
	class D3D11DynamicRHI;

	class D3D11IndexBuffer : public RHIIndexBuffer
	{
	public:
		D3D11IndexBuffer(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11IndexBuffer();

		virtual bool CreateIndexBuffer(const uint16_t* InData, int32_t InUsage, int32_t TriangleNumber) override;
		virtual bool CreateIndexBuffer(const uint32_t* InData, int32_t InUsage, int32_t TriangleNumber) override;
		virtual int32_t GetIndexFormat() const override;
		virtual int32_t GetIndexCount() const override;
	public:
		ID3D11Buffer* GetNativeBuffer() const;
	private:
		bool CreateBuffer(const void* InData, int32_t InUsage);

	private:
		D3D11IndexBufferPrivate* d_ptr = nullptr;
	};
}