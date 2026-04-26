#pragma once
#include <memory>

namespace RenderCore
{
	class FD3D12Adapter;
	class FD3D12Device;
	class D3D12CommandContext;

	// Centralizes once-per-second D3D12 runtime stats logging behind a single switch.
	// Enable with command line: d3d12_memmon=1
	class D3D12RuntimeStatsMonitor
	{
	public:
		static void TickOncePerSecond(D3D12CommandContext& Context,
			const std::shared_ptr<FD3D12Adapter>& Adapter,
			const std::shared_ptr<FD3D12Device>& Device);
	};
}

