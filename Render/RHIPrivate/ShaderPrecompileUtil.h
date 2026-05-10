#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace RenderCore
{
	struct RHIShaderMacro;

	/** FNV-style chain matching Tools/build_precompiled_shaders.py (basename + entry + profile + macros in order). */
	size_t ShaderPrecompileLookupHash(const std::string& hlslBaseFileNameUtf8, const std::string& entry, const std::string& profile,
		const std::vector<RHIShaderMacro>& macros);

	/** Pixel/compute only: loads ShaderLibDX/Built/<base>__<entry>__<profile>__<hash16>.cso (falls back to .dxbc). */
	bool TryLoadPrecompiledShaderBytecode(const std::wstring& hlslSourcePath, const std::string& entry, const std::string& profile,
		const std::vector<RHIShaderMacro>& macros, std::vector<uint8_t>& outBytecode);
}
