#pragma once

#include <cstdint>
#include <memory>

namespace RenderCore
{
	class FD3D12Adapter;
	class FD3D12Device;

	// Diagnostic logging behind d3d12_memmon=1 / d3d_mem=1. VidMem+ProcMem each tick;
	// HeapWalk + full VirtualQuery only when d3d12_memmon_deep=1 (or d3d_mem_deep=1).
	class D3D12MemoryMonitor
	{
	public:
		static bool IsEnabled();

		// Call once per second from a convenient render-thread location.
		// All output is throttled internally.
		static void TickOncePerSecond(const std::shared_ptr<FD3D12Adapter>& Adapter, const std::shared_ptr<FD3D12Device>& Device);
	};
}

