#pragma once
#include "RHI/RHIIndirectArgsBuffer.h"
#include "RHI/RHIDefinitions.h"

struct ID3D11Buffer;

namespace RenderCore
{
	class D3D11DynamicRHI;

	class D3D11IndirectArgsBuffer : public RHIIndirectArgsBuffer
	{
	public:
		explicit D3D11IndirectArgsBuffer(D3D11DynamicRHI* InRHI);
		~D3D11IndirectArgsBuffer() override;

		bool CreateBuffer(uint32_t ByteSize, EBufferUsageFlags InUsage, const void* InitialData);

		uint32_t GetByteSize() const override;
		void UpdateContents(const void* Data, uint32_t ByteOffset, uint32_t NumBytes) override;

		ID3D11Buffer* GetNativeBuffer() const;

	private:
		struct Private;
		Private* d_ptr = nullptr;
	};
}
