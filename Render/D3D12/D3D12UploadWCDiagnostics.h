#pragma once
#include <cstdint>

namespace RenderCore
{
	// Upload (WRITE_COMBINE) diagnostics. Enabled by command line: d3d12_memmon=1
	// Kept separate from allocators to avoid pulling heavy debug/symbol deps into core paths.
	void D3D12UploadWCDiagnostics_OnUploadMap(const wchar_t* Tag, void* MappedPtr, uint64_t SizeBytes);
}

