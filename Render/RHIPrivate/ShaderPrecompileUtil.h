#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace RenderCore
{
	struct RHIShaderMacro;

    /** FNV-style chain matching Tools/build_precompiled_shaders.py: basename|entry|profile, macros in order, then
	    quoted-include dependency tree raw bytes (see ShaderPrecompileQuotedIncludeTreeHash).
	    Precompiled PS macros must match runtime push order (see Render/ShaderLibDX/precompile_manifest.json comments;
	    PBR: PBRMaterialRender::InitShader; translucent forward PS: FilterMacrosTranslucentForwardPS after InitShader). */
	size_t ShaderPrecompileLookupHash(const std::string& hlslBaseFileNameUtf8, const std::string& entry, const std::string& profile,
		const std::vector<RHIShaderMacro>& macros, size_t quotedIncludeTreeHash);

	/** FNV chain over entry .hlsl + transitive "#include \"...\"" files (same-dir relative), DFS order; delimiter byte between files. */
	size_t ShaderPrecompileQuotedIncludeTreeHash(const std::wstring& hlslSourcePathAbs);

	/** Pixel/compute only: loads ShaderLibDX/Built/<base>__<entry>__<profile>__<hash16>.cso (falls back to .dxbc).
	    Pass -shaderprecompileverbose on the command line to log lookup misses (capped) and early skip reasons. */
	bool TryLoadPrecompiledShaderBytecode(const std::wstring& hlslSourcePath, const std::string& entry, const std::string& profile,
		const std::vector<RHIShaderMacro>& macros, std::vector<uint8_t>& outBytecode);
}
