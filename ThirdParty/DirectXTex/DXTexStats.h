#pragma once
#include <atomic>

#ifndef WITH_D3D12_MEMMON
#define WITH_D3D12_MEMMON 0
#endif

// Lightweight counters to detect unintended per-frame DirectXTex usage.
// These functions may allocate transient resources and command objects internally.
namespace DXTexStats
{
	inline std::atomic_uint64_t& CaptureTextureCalls_D3D12()
	{
		static std::atomic_uint64_t v{ 0 };
		return v;
	}

	inline std::atomic_uint64_t& ScreenGrab_SaveWICCalls_D3D12()
	{
		static std::atomic_uint64_t v{ 0 };
		return v;
	}

	inline std::atomic_uint64_t& ScreenGrab_SaveDDSCalls_D3D12()
	{
		static std::atomic_uint64_t v{ 0 };
		return v;
	}

	inline std::atomic_uint64_t& WICTextureLoader_LoadFromFileCalls_D3D12()
	{
		static std::atomic_uint64_t v{ 0 };
		return v;
	}

	inline std::atomic_uint64_t& DDSTextureLoader_LoadFromFileCalls_D3D12()
	{
		static std::atomic_uint64_t v{ 0 };
		return v;
	}

#if WITH_D3D12_MEMMON
	inline void TraceCaptureTexture_D3D12() { ++CaptureTextureCalls_D3D12(); }
	inline void TraceScreenGrab_SaveDDS_D3D12() { ++ScreenGrab_SaveDDSCalls_D3D12(); }
	inline void TraceScreenGrab_SaveWIC_D3D12() { ++ScreenGrab_SaveWICCalls_D3D12(); }
	inline void TraceWICTextureLoader_LoadFromFile_D3D12() { ++WICTextureLoader_LoadFromFileCalls_D3D12(); }
	inline void TraceDDSTextureLoader_LoadFromFile_D3D12() { ++DDSTextureLoader_LoadFromFileCalls_D3D12(); }
#else
	inline void TraceCaptureTexture_D3D12() {}
	inline void TraceScreenGrab_SaveDDS_D3D12() {}
	inline void TraceScreenGrab_SaveWIC_D3D12() {}
	inline void TraceWICTextureLoader_LoadFromFile_D3D12() {}
	inline void TraceDDSTextureLoader_LoadFromFile_D3D12() {}
#endif
}

