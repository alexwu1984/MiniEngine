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
#include <functional>
#include <atomic>
#include <unordered_set>
#include <vector>

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

	/** Shadow pass pairs ShadowPass-VS (JIT) with ShadowPass-PS; keep PS JIT until offline/shadow manifest parity is fully validated. */
	static bool MeshPixelShaderSkipPrecompiled(const std::filesystem::path& filename)
	{
		return filename.wstring() == L"ShadowPass-PS.hlsl";
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

	/** Same rules as Tools/build_precompiled_shaders.py INCLUDE_QUOTED + collect_hlsl_sources (trimmed line). */
	static bool ParseQuotedIncludeTrimmed(const std::string& trimmed, std::string& outRelPath)
	{
		size_t i = 0;
		while (i < trimmed.size() && (trimmed[i] == ' ' || trimmed[i] == '\t'))
			++i;
		if (i >= trimmed.size() || trimmed[i] != '#')
			return false;
		++i;
		while (i < trimmed.size() && (trimmed[i] == ' ' || trimmed[i] == '\t'))
			++i;
		static constexpr char kInc[] = "include";
		if (i + sizeof(kInc) - 1 > trimmed.size() || trimmed.compare(i, sizeof(kInc) - 1, kInc) != 0)
			return false;
		i += sizeof(kInc) - 1;
		while (i < trimmed.size() && (trimmed[i] == ' ' || trimmed[i] == '\t'))
			++i;
		if (i >= trimmed.size() || trimmed[i] != '"')
			return false;
		++i;
		const size_t start = i;
		while (i < trimmed.size() && trimmed[i] != '"')
			++i;
		if (i >= trimmed.size())
			return false;
		outRelPath.assign(trimmed.data() + start, trimmed.data() + i);
		return true;
	}

	static void CollectQuotedIncludeOrder(const std::filesystem::path& entryCanonical, std::vector<std::filesystem::path>& ordered)
	{
		std::unordered_set<std::string> seenUtf8;
		std::function<void(const std::filesystem::path&)> visit = [&](const std::filesystem::path& rawPath) {
			std::error_code ec;
			const std::filesystem::path canon = std::filesystem::weakly_canonical(rawPath, ec);
			const std::string key = core::ucs2_u8(canon.wstring());
			if (!seenUtf8.insert(key).second)
				return;
			ordered.push_back(canon);
			if (!std::filesystem::is_regular_file(canon))
				return;
			std::ifstream fin(canon, std::ios::binary);
			if (!fin.good())
				return;
			std::string line;
			while (std::getline(fin, line))
			{
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				size_t a = 0;
				while (a < line.size() && (line[a] == ' ' || line[a] == '\t'))
					++a;
				size_t b = line.size();
				while (b > a && (line[b - 1] == ' ' || line[b - 1] == '\t'))
					--b;
				if (a >= b)
					continue;
				const std::string trimmed = line.substr(a, b - a);
				std::string rel;
				if (!ParseQuotedIncludeTrimmed(trimmed, rel))
					continue;
				std::error_code ec2;
				const std::filesystem::path childCanon = std::filesystem::weakly_canonical(canon.parent_path() / rel, ec2);
				if (std::filesystem::is_regular_file(childCanon))
					visit(childCanon);
			}
		};
		visit(entryCanonical);
	}
}

namespace RenderCore
{
	size_t ShaderPrecompileQuotedIncludeTreeHash(const std::wstring& hlslSourcePathAbs)
	{
		std::error_code ec;
		const std::filesystem::path entryCanon = std::filesystem::weakly_canonical(std::filesystem::path(hlslSourcePathAbs), ec);
		std::vector<std::filesystem::path> ordered;
		CollectQuotedIncludeOrder(entryCanon, ordered);
		size_t h = HASH_SEED;
		const uint8_t delim = 0;
		for (const std::filesystem::path& p : ordered)
		{
			if (!std::filesystem::is_regular_file(p))
				continue;
			std::ifstream in(p, std::ios::binary | std::ios::ate);
			if (!in.good())
				continue;
			const std::streamsize sz = in.tellg();
			if (sz < 0)
				continue;
			in.seekg(0, std::ios::beg);
			std::vector<char> buf(static_cast<size_t>(sz));
			if (sz > 0 && !in.read(buf.data(), sz))
				continue;
			h = core::Hash(buf.data(), static_cast<size_t>(sz), h);
			h = core::Hash(&delim, 1, h);
		}
		return h;
	}

	size_t ShaderPrecompileLookupHash(const std::string& hlslBaseFileNameUtf8, const std::string& entry, const std::string& profile,
		const std::vector<RHIShaderMacro>& macros, size_t quotedIncludeTreeHash)
	{
		std::string key = hlslBaseFileNameUtf8 + "|" + entry + "|" + profile;
		size_t h = core::HashString(key);
		for (const RHIShaderMacro& m : macros)
		{
			h = core::HashString(m.Name, h);
			h = core::HashString(m.Definition, h);
		}
		h = core::Hash(&quotedIncludeTreeHash, sizeof(quotedIncludeTreeHash), h);
		return h;
	}

	static std::string Hex16(size_t v)
	{
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(v));
		return buf;
	}

	static bool ShaderPrecompileVerboseCli()
	{
		return core::CommandLine::Get().GetSwitch("shaderprecompileverbose");
	}

	bool TryLoadPrecompiledShaderBytecode(const std::wstring& hlslSourcePath, const std::string& entry, const std::string& profile,
		const std::vector<RHIShaderMacro>& macros, std::vector<uint8_t>& outBytecode)
	{
		outBytecode.clear();
		if (core::CommandLine::Get().GetSwitch("shaderjit"))
		{
			static std::atomic<bool> s_loggedJit{false};
			if (ShaderPrecompileVerboseCli() && !s_loggedJit.exchange(true))
				core::inf() << "[ShaderPrecompile] skip: shaderjit (all JIT)";
			return false;
		}
		// Vertex shaders are still JIT with these flags; mixing debug JIT VS + optimized precompiled PS breaks I/O signature -> CreateGraphicsPipelineState fails.
		if (RuntimeShaderCompileFlagsForJitParity() != 0)
		{
			static std::atomic<bool> s_loggedDebugParity{false};
			if (ShaderPrecompileVerboseCli() && !s_loggedDebugParity.exchange(true))
				core::inf() << "[ShaderPrecompile] skip: shaderdebug JIT parity (precompiled disabled)";
			return false;
		}

		std::error_code ec;
		const std::filesystem::path src(hlslSourcePath);
		if (LooksLikePixelProfile(profile) && MeshPixelShaderSkipPrecompiled(src.filename()))
		{
			static std::atomic<bool> s_loggedMeshPsJit{false};
			if (!s_loggedMeshPsJit.exchange(true))
				core::inf() << "[ShaderPrecompile] ShadowPass-PS JIT only (VS/PS parity); PBRMaterial/TranslucentPBRForward use precompiled when manifest matches InitShader macro order";
			return false;
		}
		const std::filesystem::path builtDir = src.parent_path() / L"Built";
		const std::string baseUtf8 = core::ucs2_u8(src.filename().wstring());
		const size_t treeHash = ShaderPrecompileQuotedIncludeTreeHash(hlslSourcePath);
		const size_t keyHash = ShaderPrecompileLookupHash(baseUtf8, entry, profile, macros, treeHash);
		const std::wstring baseName = core::u8_ucs2(baseUtf8 + "__" + entry + "__" + profile + "__" + Hex16(keyHash));
		std::filesystem::path builtPath = builtDir / (baseName + L".cso");
		std::ifstream in(builtPath.wstring(), std::ios::binary | std::ios::ate);
		if (!in.good())
		{
			in.clear();
			builtPath = builtDir / (baseName + L".dxbc");
			in.open(builtPath.wstring(), std::ios::binary | std::ios::ate);
			if (!in.good())
			{
				if (ShaderPrecompileVerboseCli())
				{
					static std::atomic<int> s_missDetailLogs{0};
					if (s_missDetailLogs.fetch_add(1) < 48)
					{
						const std::filesystem::path tryCso = builtDir / (baseName + L".cso");
						core::inf() << "[ShaderPrecompile] miss entry=" << entry << " profile=" << profile << " tree=" << Hex16(treeHash)
									<< " key=" << Hex16(keyHash) << " hlsl=" << core::ucs2_u8(src.wstring())
									<< " try_cso=" << core::ucs2_u8(tryCso.wstring());
					}
				}
				return false;
			}
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
