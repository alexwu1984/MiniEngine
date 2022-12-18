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