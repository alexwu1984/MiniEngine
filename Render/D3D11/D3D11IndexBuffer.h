#pragma once
#include "RHI/RHIIndexBuffer.h"


namespace RenderCore 
{
	struct D3D11IndexBufferP;
	class D3D11DynamicRHI;

	class D3D11IndexBuffer : public RHIIndexBuffer
	{
	public:
		D3D11IndexBuffer(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11IndexBuffer();


		virtual bool CreateIndexBuffer(const uint16_t* InData, int32_t InUsage, int32_t IndexCount) override;
		virtual bool CreateIndexBuffer(const uint32_t* InData, int32_t InUsage, int32_t IndexCount) override;

	private:
		bool CreateBuffer(const void* InData, int32_t InUsage);

	private:
		std::shared_ptr< D3D11IndexBufferP> Impl;
	};
}