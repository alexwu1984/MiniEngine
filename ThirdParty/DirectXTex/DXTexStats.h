#pragma once
#include <atomic>

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
}

