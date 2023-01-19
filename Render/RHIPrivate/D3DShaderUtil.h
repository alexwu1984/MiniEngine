#pragma once
#include "core/inc.h"
#include "win/com_ptr.h"
#include <D3Dcompiler.h>

namespace RenderCore
{
	struct RHIShaderMacro;
	class ShaderUtil
	{
	public:
		static win32::com_ptr<ID3DBlob> CreateShader(const std::wstring& ShaderFile, const std::string& EntryPoint, const std::string& TargetModel, const D3D_SHADER_MACRO* pDefines);
		static void RHIShaderMarcoToD3DShaderMacro(const std::vector<RHIShaderMacro>& RHIShaderMacros, std::vector<D3D_SHADER_MACRO>& D3DShaderMacros);
	};
	
}