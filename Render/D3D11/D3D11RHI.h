#pragma once
#include "RHI/DynamicRHI.h"


struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGIFactory1;

namespace RenderCore
{
	struct D3D11DynamicRHIP;

	class D3D11DynamicRHI : public DynamicRHI
	{
	public:
		D3D11DynamicRHI();
		virtual ~D3D11DynamicRHI();

		/** Initializes the RHI; separate from IDynamicRHIModule::CreateRHI so that GDynamicRHI is set when it is called. */
		virtual void Init() override;

		/** Shutdown the RHI; handle shutdown and resource destruction before the RHI's actual destructor is called (so that all resources of the RHI are still available for shutdown). */
		virtual void Shutdown() override;

		virtual const TCHAR* GetName() { return TEXT("D3D11"); }

		virtual std::shared_ptr< RHIViewPort> RHICreateViewport(void* WindowHandle, uint32_t SizeX, uint32_t SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) override;
		virtual std::shared_ptr< RHIVertexBuffer> RHICreateVertexBuffer(const void* Data, EBufferUsageFlags InUsage, int32_t StrideByteWidth, int32_t Count) override;
		virtual void RHIUpdateVertexBuffer(std::shared_ptr< RHIVertexBuffer> VertexBuffer,const void* InData, int32_t nVertex, int32_t sizePerVertex) override;
		virtual std::shared_ptr< RHIIndexBuffer> RHICreateIndexBuffer(const uint16_t* InData, EBufferUsageFlags InUsage, int32_t IndexCount) override;
		virtual std::shared_ptr< RHIIndexBuffer> RHICreateIndexBuffer(const uint32_t* InData, EBufferUsageFlags InUsage, int32_t IndexCount) override;

		// Accessors.
		ID3D11Device* GetDevice() const;
		ID3D11DeviceContext* GetDeviceContext() const;
		IDXGIFactory1* GetFactory() const;

	private:
		bool InitD3DDevice();
		bool FindAdapter();

	private:
		std::shared_ptr< D3D11DynamicRHIP> Data;
	};
}