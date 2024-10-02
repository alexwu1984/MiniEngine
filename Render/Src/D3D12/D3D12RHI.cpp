#include "D3D12/D3D12RHI.h"
#include "win/com_ptr.h"
#include "RHIPrivate/D3D12RHIPrivate.h"

namespace RenderCore
{

	bool D3D12DynamicRHIModule::IsSupported()
	{
		return false;
	}

	std::shared_ptr<DynamicRHI> D3D12DynamicRHIModule::CreateRHI()
	{
		return {};
	}


	void D3D12DynamicRHIModule::FindAdapter()
	{
		assert(ChosenAdapters.size() == 0);

		// Try to create the DXGIFactory.  This will fail if we're not running Vista.
		win32::com_ptr<IDXGIFactory4> DXGIFactory;
		SafeCreateDXGIFactory(DXGIFactory.getpp());
		if (!DXGIFactory)
		{
			return;
		}

		bool bAllowPerfHUD = true;
	}

	D3D12DynamicRHI::D3D12DynamicRHI()
	{

	}

	D3D12DynamicRHI::~D3D12DynamicRHI()
	{

	}

	void D3D12DynamicRHI::Init()
	{

	}

	void D3D12DynamicRHI::Shutdown()
	{

	}

	std::shared_ptr<RHICommandContext> D3D12DynamicRHI::GetDefaultCommandContext()
	{
		return {};
	}

}