#include "RHIPrivate/ShaderPrecompileUtil.h"
#include "RHIPrivate/D3DShaderUtil.h"
#include "RHI/RHIShdader.h"
#include "core/commandline.h"
#include "core/logger.h"
#include "core/strings.h"
#include "win/com_ptr.h"
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <unordered_set>
#include <string>
#include <vector>

namespace
{
	/** FNV-1a over raw bytes (matches Tools/build_precompiled_shaders.py hash_bytes: 64-bit state, prime 16777619). */
	static uint64_t ShaderPrecompileHashBytesFNV64(const void* ptr, size_t size, uint64_t result)
	{
		const auto* p = static_cast<const uint8_t*>(ptr);
		for (size_t i = 0; i < size; ++i)
		{
			result = (result * 16777619ull) ^ static_cast<uint64_t>(p[i]);
		}
		return result;
	}

	/** Match Python hash_string(s, h) — UTF-8 bytes via std::string contents (do not use core::HashString: signed char on bytes). */
	static uint64_t ShaderPrecompileHashUtf8StringLikePython(const std::string& s, uint64_t h)
	{
		return ShaderPrecompileHashBytesFNV64(s.data(), s.size(), h);
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
	uint64_t ShaderPrecompileQuotedIncludeTreeHash(const std::wstring& hlslSourcePathAbs)
	{
		std::error_code ec;
		const std::filesystem::path entryCanon = std::filesystem::weakly_canonical(std::filesystem::path(hlslSourcePathAbs), ec);
		std::vector<std::filesystem::path> ordered;
		CollectQuotedIncludeOrder(entryCanon, ordered);
		uint64_t h = static_cast<uint64_t>(HASH_SEED);
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
			h = ShaderPrecompileHashBytesFNV64(buf.data(), static_cast<size_t>(sz), h);
			h = ShaderPrecompileHashBytesFNV64(&delim, 1, h);
		}
		return h;
	}

	static char ReadOneTierChar(const std::filesystem::path& tierPath)
	{
		std::ifstream fin(tierPath, std::ios::binary);
		if (!fin.good())
			return '\0';
		char c = '\0';
		fin.read(&c, 1);
		if (fin.gcount() != 1)
			return '\0';
		if (c == '0' || c == '1' || c == '2')
			return c;
		return '\0';
	}

	/** Per VS bucket: Debug=tier2 (SKIP_OPTIMIZATION); Release bucket = non-Debug configs, tier0 (full FXC opt). Legacy file if missing. */
	static char ReadPrecompileFxCompileTierChar(const std::filesystem::path& builtDir)
	{
		std::error_code ec;
		if (!std::filesystem::is_directory(builtDir, ec))
			return '1';
#if defined(_DEBUG)
		const std::filesystem::path primary = builtDir / L".precompile_fxc_tier.Debug";
#else
		const std::filesystem::path primary = builtDir / L".precompile_fxc_tier.Release";
#endif
		char c = ReadOneTierChar(primary);
		if (c != '\0')
			return c;
		const std::filesystem::path legacy = builtDir / L".precompile_fxc_tier";
		c = ReadOneTierChar(legacy);
		if (c != '\0')
			return c;
#if defined(_DEBUG)
		return '2';
#else
		return '0';
#endif
	}

	uint64_t ShaderPrecompileLookupHash(const std::string& hlslBaseFileNameUtf8, const std::string& entry, const std::string& profile,
		const std::vector<RHIShaderMacro>& macros, uint64_t quotedIncludeTreeHash, char compileTier)
	{
		const std::string key =
			hlslBaseFileNameUtf8 + "|" + entry + "|" + profile + "|cpt=" + std::string(1, compileTier);
		uint64_t h = ShaderPrecompileHashUtf8StringLikePython(key, static_cast<uint64_t>(HASH_SEED));
		for (const RHIShaderMacro& m : macros)
		{
			h = ShaderPrecompileHashUtf8StringLikePython(m.Name, h);
			h = ShaderPrecompileHashUtf8StringLikePython(m.Definition, h);
		}
		h = ShaderPrecompileHashBytesFNV64(&quotedIncludeTreeHash, sizeof(quotedIncludeTreeHash), h);
		return h;
	}

	static std::string Hex16(uint64_t v)
	{
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(v));
		return std::string(buf);
	}

	bool TryLoadPrecompiledShaderBytecode(const std::wstring& hlslSourcePath, const std::string& entry, const std::string& profile,
		const std::vector<RHIShaderMacro>& macros, std::vector<uint8_t>& outBytecode)
	{
		outBytecode.clear();
		if (core::CommandLine::Get().GetSwitch("shaderjit"))
			return false;
		// Non-zero FXC flags (e.g. shaderdebug=1 in Debug) force full JIT so offline PS bytecode matches VS compile flags.
		if (ShaderUtil::GetD3DCompileFlagsForBuild() != 0)
			return false;

		std::error_code ec;
		const std::filesystem::path src(hlslSourcePath);
		const std::filesystem::path builtDir = src.parent_path() / L"Built";
		const std::string baseUtf8 = core::ucs2_u8(src.filename().wstring());
		const uint64_t treeHash = ShaderPrecompileQuotedIncludeTreeHash(hlslSourcePath);
		const char compileTier = ReadPrecompileFxCompileTierChar(builtDir);
		const uint64_t keyHash = ShaderPrecompileLookupHash(baseUtf8, entry, profile, macros, treeHash, compileTier);
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
