#include "D3D12/D3D12Shaders.h"
#include "RHIPrivate/D3DShaderUtil.h"
#include "RHIPrivate/ShaderCore.h"

namespace RenderCore
{

	FD3D12VertexShader::FD3D12VertexShader()
		:RHIVertexShader(SF_Vertex)
	{

	}

	bool FD3D12VertexShader::CreateShader(const std::wstring& FileName, const std::string& VSMain, 
											const RHIVertexDeclare& VertexDeclare, const std::vector<RHIShaderMacro>& MacroDefines)
	{
		std::vector< D3D_SHADER_MACRO> D3DShaderMacros;
		ShaderUtil::RHIShaderMarcoToD3DShaderMacro(MacroDefines, D3DShaderMacros);

		Code = ShaderUtil::CompileShader(FileName, D3DShaderMacros.data(), VSMain, "vs_5_0");
		if (!Code.empty())
		{
			return false;
		}
		
		uint32_t NumSamplers = 0;
		uint32_t NumSRVs = 0;
		uint32_t NumCBs = 0;
		uint32_t NumUAVs = 0;
		FShaderCompilerOutput Output;

		ShaderUtil::ExtractParameterMapFromD3DShader(0, Code, NumSamplers, NumSRVs, NumCBs, NumUAVs, Output);

		ResourceCounts.NumCBs = NumCBs;
		ResourceCounts.NumSRVs = NumSRVs;
		ResourceCounts.NumUAVs = NumUAVs;
		ResourceCounts.NumSamplers = NumSamplers;
		return true;
	}

}
