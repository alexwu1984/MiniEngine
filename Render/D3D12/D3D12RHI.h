#pragma once
#include "RHI/DynamicRHI.h"
#include "win/com_ptr.h"
#include "d3dx12.h"

namespace RenderCore
{
	class D3D12DynamicRHI;
	class D3D12Adapter;
	class D3D12Device;

	class D3D12DynamicRHIModule : public IDynamicRHIModule
	{
	public:
		D3D12DynamicRHIModule() = default;
		~D3D12DynamicRHIModule() = default;
		bool IsSupported() override;
		std::shared_ptr<DynamicRHI> CreateRHI() override;
	private:
		std::shared_ptr< D3D12DynamicRHI> DynamicRHI;
		std::vector<std::shared_ptr<D3D12Adapter>> ChosenAdapters;

		// set MaxSupportedFeatureLevel and ChosenAdapter
		void FindAdapter();
	};

	class D3D12DynamicRHI : public DynamicRHI,std::enable_shared_from_this<D3D12DynamicRHI>
	{
	public:
		D3D12DynamicRHI();
		virtual ~D3D12DynamicRHI();

		/** Initializes the RHI; separate from IDynamicRHIModule::CreateRHI so that GDynamicRHI is set when it is called. */
		virtual void Init() override;

		/** Shutdown the RHI; handle shutdown and resource destruction before the RHI's actual destructor is called (so that all resources of the RHI are still available for shutdown). */
		virtual void Shutdown() override;

		virtual const TCHAR* GetName() { return TEXT("D3D12"); }

		virtual std::shared_ptr< RHICommandContext> GetDefaultCommandContext() override;

		win32::com_ptr<ID3D12CommandQueue> CreateCommandQueue(D3D12Device* Device, const D3D12_COMMAND_QUEUE_DESC& Desc);
	};
}