#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace RenderCore
{
	struct RHIShaderMacro;

    /** FNV-style chain matching Tools/build_precompiled_shaders.py: basename|entry|profile, macros in order, then
	    quoted-include dependency tree raw bytes (see ShaderPrecompileQuotedIncludeTreeHash).
	    Macros must match runtime push order (maintain Tools/gen_precompile_manifest.py; shadow: ShadowPS.cpp InitResource;
	    PBR: PBRMaterialRender::InitShader; translucent PS: FilterMacrosTranslucentForwardPS). */
	uint64_t ShaderPrecompileLookupHash(const std::string& hlslBaseFileNameUtf8, const std::string& entry, const std::string& profile,
		const std::vector<RHIShaderMacro>& macros, uint64_t quotedIncludeTreeHash);

	/** FNV chain over entry .hlsl + transitive "#include \"...\"" files (same-dir relative), DFS order; delimiter byte between files. */
	uint64_t ShaderPrecompileQuotedIncludeTreeHash(const std::wstring& hlslSourcePathAbs);

	/** Loads ShaderLibDX/Built/<base>__<entry>__<profile>__<hash16>.cso (falls back to .dxbc); VS/PS/CS per manifest. */
	bool TryLoadPrecompiledShaderBytecode(const std::wstring& hlslSourcePath, const std::string& entry, const std::string& profile,
		const std::vector<RHIShaderMacro>& macros, std::vector<uint8_t>& outBytecode);
}
