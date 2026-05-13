#pragma once
#include "RHI/RHIStructuredBuffer.h"
#include "RHIPrivate/D3D11RHIDeclare.h"

namespace RenderCore
{
	class D3D11DynamicRHI;
	struct D3D11StructuredBufferPrivate;

	/**
	 * D3D11 backing for `RHIStructuredBuffer`. Single ID3D11Buffer per instance, with SRV always present and an
	 * optional UAV created when the caller passes BUF_UnorderedAccess. Driver renaming on D3D11_USAGE_DYNAMIC
	 * (Map WRITE_DISCARD) supplies the cross-frame safety that D3D12 fulfils via the ring slots.
	 *   - BUF_Static                       : D3D11_USAGE_IMMUTABLE/DEFAULT, SRV-only.
	 *   - BUF_Dynamic                      : D3D11_USAGE_DYNAMIC + WRITE_DISCARD on update, SRV-only.
	 *   - BUF_Static | BUF_UnorderedAccess : D3D11_USAGE_DEFAULT + UAV; UpdateStructuredBuffer not supported.
	 *   - BUF_Dynamic | BUF_UnorderedAccess: rejected (D3D11 forbids the combination).
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
		virtual bool HasUAV() const override;

		ID3D11ShaderResourceView* GetSRV() const;
		/** Valid only when created with BUF_UnorderedAccess. */
		ID3D11UnorderedAccessView* GetUAV() const;
		ID3D11Buffer* GetNativeBuffer() const;

	private:
		D3D11StructuredBufferPrivate* d_ptr = nullptr;
	};
}
