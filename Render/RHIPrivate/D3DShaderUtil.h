#pragma once
#include "core/inc.h"
#include "win/com_ptr.h"
#include <D3Dcompiler.h>
#include <d3d12shader.h>

namespace RenderCore
{
	struct FShaderCompilerOutput;

	struct RHIShaderMacro;
	class ShaderUtil
	{
	public:
		static win32::com_ptr<ID3DBlob> CreateShader(const std::wstring& ShaderFile, const std::string& EntryPoint, const std::string& TargetModel, const D3D_SHADER_MACRO* pDefines);
		static void RHIShaderMarcoToD3DShaderMacro(const std::vector<RHIShaderMacro>& RHIShaderMacros, std::vector<D3D_SHADER_MACRO>& D3DShaderMacros);

		static HRESULT D3DCreateReflectionFromBlob(ID3DBlob* DxilBlob, win32::com_ptr<ID3D12ShaderReflection>& OutReflection);
		static void ExtractParameterMapFromD3DShader(uint32_t BindingSpace, const std::vector<uint8_t>& Code,
			uint32_t& NumSamplers, uint32_t& NumSRVs, uint32_t& NumCBs, uint32_t& NumUAVs, FShaderCompilerOutput& Output);
		static std::vector<uint8_t> CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines,
			const std::string& entrypoint, const std::string& target);
	};
	
}