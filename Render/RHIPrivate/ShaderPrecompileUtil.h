#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace RenderCore
{
	struct RHIShaderMacro;

    /** FNV-style chain matching Tools/build_precompiled_shaders.py: basename|entry|profile, macros in order, then
	    quoted-include dependency tree raw bytes (see ShaderPrecompileQuotedIncludeTreeHash).
	    Manifest lists GLFFViewer material variants with and without RHI_BINDLESS so D3D11 and D3D12 can both hit .cso.
	    Runtime pushes RHI_BINDLESS when MaterialBase::WantsRHIBindless() and GetRHIAPIType()==E_D3D12 (see PBRMaterialRender::InitShader). */
	uint64_t ShaderPrecompileLookupHash(const std::string& hlslBaseFileNameUtf8, const std::string& entry, const std::string& profile,
		const std::vector<RHIShaderMacro>& macros, uint64_t quotedIncludeTreeHash);

	/** FNV chain over entry .hlsl + transitive "#include \"...\"" files (same-dir relative), DFS order; delimiter byte between files. */
	uint64_t ShaderPrecompileQuotedIncludeTreeHash(const std::wstring& hlslSourcePathAbs);

	/** Loads ShaderLibDX/Built/<base>__<entry>__<profile>__<hash16>.cso (falls back to .dxbc); VS/PS/CS per manifest. */
	bool TryLoadPrecompiledShaderBytecode(const std::wstring& hlslSourcePath, const std::string& entry, const std::string& profile,
		const std::vector<RHIShaderMacro>& macros, std::vector<uint8_t>& outBytecode);
}
