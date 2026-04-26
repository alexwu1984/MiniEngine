#pragma once

#include <cstdint>
#include <memory>

namespace RenderCore
{
	class FD3D12Adapter;
	class FD3D12Device;

	// Centralizes all "memory related" diagnostic logging behind a single switch.
	// Enable with command line: d3d12_memmon=1
	class D3D12MemoryMonitor
	{
	public:
		static bool IsEnabled();

		// Call once per second from a convenient render-thread location.
		// All output is throttled internally.
		static void TickOncePerSecond(const std::shared_ptr<FD3D12Adapter>& Adapter, const std::shared_ptr<FD3D12Device>& Device);
	};
}

