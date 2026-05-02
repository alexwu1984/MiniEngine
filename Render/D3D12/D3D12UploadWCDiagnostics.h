#pragma once
#include <cstdint>
#include <cstddef>

namespace RenderCore
{
	// Upload (WRITE_COMBINE) diagnostics. Enabled by command line: d3d12_memmon=1
	// Kept separate from allocators to avoid pulling heavy debug/symbol deps into core paths.
	void D3D12UploadWCDiagnostics_OnUploadMap(const wchar_t* Tag, void* MappedPtr, uint64_t SizeBytes);

	// Track mapped WC regions and attribute "gradual commit" growth.
	// Register is cheap; dump does VirtualQuery scanning (once per second in memmon mode).
	void D3D12UploadWCDiagnostics_RegisterMappedRegion(const wchar_t* Tag, void* BasePtr, uint64_t SizeBytes);
	void D3D12UploadWCDiagnostics_DumpMappedRegionCommitDeltas();

	// Fallback attribution: scan the full process VA space and aggregate committed WC bytes by AllocationBase.
	// Useful when WC growth comes from WC regions that are not explicitly mapped / registered.
	void D3D12UploadWCDiagnostics_DumpProcessWideWcCommitDeltas();

	// Large-page allocation diagnostics (typically UPLOAD/WC). Enabled by command line: d3d12_memmon=1
	// Aggregated and logged at most once per second.
	void D3D12UploadWCDiagnostics_OnAllocateLargePage(const wchar_t* Tag, std::size_t SizeBytes);

	// CreateCommittedResource diagnostics for large UPLOAD buffers (where D3D11 would often hide staging).
	// Aggregated and logged at most once per second.
	void D3D12UploadWCDiagnostics_OnCreateUploadCommittedBuffer(const wchar_t* Tag, std::size_t SizeBytes);
}

