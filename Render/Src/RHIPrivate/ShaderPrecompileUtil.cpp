#include "RHIPrivate/ShaderPrecompileUtil.h"
#include "RHI/RHIShdader.h"
#include "core/commandline.h"
#include "core/logger.h"
#include "core/strings.h"
#include "win/com_ptr.h"
#include <d3dcompiler.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <atomic>

namespace
{
	static bool LooksLikePixelProfile(const std::string& profile)
	{
		if (profile.size() < 3)
			return false;
		const unsigned char a = static_cast<unsigned char>(profile[0]);
		const unsigned char b = static_cast<unsigned char>(profile[1]);
		return (a == 'p' || a == 'P') && (b == 's' || b == 'S') && profile[2] == '_';
	}

	/** Pixel shaders that consume VS_OUTPUT_SCENE from JIT vertex factories; skip offline .cso to avoid VS/PS signature drift (E_INVALIDARG PSO). */
	static bool MeshPixelShaderSkipPrecompiled(const std::filesystem::path& filename)
	{
		const std::wstring fn = filename.wstring();
		return fn == L"ShadowPass-PS.hlsl" || fn == L"PBRMaterial.hlsl" || fn == L"TranslucentPBRForward.hlsl";
	}

	/** Reject truncated/non-DXBC/corrupt files so we JIT instead of building a broken root signature / PSO. */
	static bool PrecompiledBlobLooksLikeReflectableDxbc(const std::vector<uint8_t>& bc)
	{
		if (bc.size() < 28)
			return false;
		if (bc[0] != 'D' || bc[1] != 'X' || bc[2] != 'B' || bc[3] != 'C')
			return false;
		win32::com_ptr<ID3D12ShaderReflection> reflector;
		const HRESULT hr = ::D3DReflect(bc.data(), bc.size(), IID_PPV_ARGS(reflector.get_init_ref()));
		return SUCCEEDED(hr) && reflector.is_valid();
	}

	/** Must stay in sync with D3DShaderUtil.cpp GetD3DCompileFlagsForBuild(): offline .cso uses flags=0 only. */
	UINT RuntimeShaderCompileFlagsForJitParity()
	{
#if defined(DEBUG) || defined(_DEBUG)
		if (core::CommandLine::Get().GetSwitch("shaderdebug"))
			return D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
		return 0;
	}
}

namespace RenderCore
{
	size_t ShaderPrecompileLookupHash(const std::string& hlslBaseFileNameUtf8, const std::string& entry, const std::string& profile,
		const std::vector<RHIShaderMacro>& macros)
	{
		std::string key = hlslBaseFileNameUtf8 + "|" + entry + "|" + profile;
		size_t h = core::HashString(key);
		for (const RHIShaderMacro& m : macros)
		{
			h = core::HashString(m.Name, h);
			h = core::HashString(m.Definition, h);
		}
		return h;
	}

	static std::string Hex16(size_t v)
	{
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(v));
		return buf;
	}

	bool TryLoadPrecompiledShaderBytecode(const std::wstring& hlslSourcePath, const std::string& entry, const std::string& profile,
		const std::vector<RHIShaderMacro>& macros, std::vector<uint8_t>& outBytecode)
	{
		outBytecode.clear();
		if (core::CommandLine::Get().GetSwitch("shaderjit"))
			return false;
		// Vertex shaders are still JIT with these flags; mixing debug JIT VS + optimized precompiled PS breaks I/O signature -> CreateGraphicsPipelineState fails.
		if (RuntimeShaderCompileFlagsForJitParity() != 0)
			return false;

		std::error_code ec;
		const std::filesystem::path src(hlslSourcePath);
		if (LooksLikePixelProfile(profile) && MeshPixelShaderSkipPrecompiled(src.filename()))
		{
			static std::atomic<bool> s_loggedMeshPsJit{false};
			if (!s_loggedMeshPsJit.exchange(true))
				core::inf() << "[ShaderPrecompile] mesh pixel shaders JIT for VS/PS linkage parity (ShadowPass-PS, PBRMaterial, TranslucentPBRForward)";
			return false;
		}
		const std::filesystem::path builtDir = src.parent_path() / L"Built";
		const std::string baseUtf8 = core::ucs2_u8(src.filename().wstring());
		const size_t keyHash = ShaderPrecompileLookupHash(baseUtf8, entry, profile, macros);
		const std::wstring baseName = core::u8_ucs2(baseUtf8 + "__" + entry + "__" + profile + "__" + Hex16(keyHash));
		std::filesystem::path builtPath = builtDir / (baseName + L".cso");
		std::ifstream in(builtPath.wstring(), std::ios::binary | std::ios::ate);
		if (!in.good())
		{
			in.clear();
			builtPath = builtDir / (baseName + L".dxbc");
			in.open(builtPath.wstring(), std::ios::binary | std::ios::ate);
			if (!in.good())
				return false;
		}
		const std::streamsize sz = in.tellg();
		if (sz <= 0)
		{
			core::err() << "[ShaderPrecompile] empty or invalid size: " << core::ucs2_u8(builtPath.wstring());
			return false;
		}
		in.seekg(0, std::ios::beg);
		outBytecode.resize(static_cast<size_t>(sz));
		if (!in.read(reinterpret_cast<char*>(outBytecode.data()), sz))
		{
			core::err() << "[ShaderPrecompile] disk read failed: " << core::ucs2_u8(builtPath.wstring());
			outBytecode.clear();
			return false;
		}

		if (!PrecompiledBlobLooksLikeReflectableDxbc(outBytecode))
		{
			core::err() << "[ShaderPrecompile] invalid or unreadable precompiled blob (will JIT): "
						<< core::ucs2_u8(builtPath.wstring());
			outBytecode.clear();
			return false;
		}

		core::inf() << "[ShaderPrecompile] loaded " << core::ucs2_u8(builtPath.wstring());
		return true;
	}
}
