#pragma once
#include "RHI/RHIStructuredBuffer.h"
#include "RHIPrivate/D3D11RHIDeclare.h"

namespace RenderCore
{
	class D3D11DynamicRHI;
	struct D3D11StructuredBufferPrivate;

	/**
	 * D3D11 backing for `RHIStructuredBuffer`. PR1: a single ID3D11Buffer + ID3D11ShaderResourceView per instance.
	 *   - BUF_Static  : D3D11_USAGE_IMMUTABLE/DEFAULT (depending on InitialData); UpdateStructuredBuffer is unsupported.
	 *   - BUF_Dynamic : D3D11_USAGE_DYNAMIC + WRITE_DISCARD on update.
	 */
	class D3D11StructuredBuffer : public RHIStructuredBuffer
	{
	public:
		explicit D3D11StructuredBuffer(D3D11DynamicRHI* InRHI);
		virtual ~D3D11StructuredBuffer();

		virtual bool CreateStructuredBuffer(uint32_t ElementStride, uint32_t ElementCount, EBufferUsageFlags Usage, const void* InitialData) override;
		virtual void UpdateStructuredBuffer(const void* Contents, uint32_t SizeInBytes) override;

		virtual uint32_t GetElementStride() const override;
		virtual uint32_t GetElementCount() const override;

		ID3D11ShaderResourceView* GetSRV() const;
		ID3D11Buffer* GetNativeBuffer() const;

	private:
		D3D11StructuredBufferPrivate* d_ptr = nullptr;
	};
}
