#include "D3D12/D3D12Shaders.h"
#include "RHIPrivate/D3DShaderUtil.h"
#include "RHIPrivate/ShaderCore.h"
#include "common/crc.h"
#include <filesystem>
#include "core/strings.h"

namespace RenderCore
{
	static size_t HashShaderMacros(const std::vector<RHIShaderMacro>& MacroDefines, size_t Hash)
	{
		for (const RHIShaderMacro& Macro : MacroDefines)
		{
			Hash = core::HashString(Macro.Name, Hash);
			Hash = core::HashString(Macro.Definition, Hash);
		}
		return Hash;
	}

	FD3D12VertexShader::FD3D12VertexShader()
		:RHIVertexShader(SF_Vertex)
	{

	}

	bool FD3D12VertexShader::CreateShader(const std::wstring& FileName, const std::string& VSMain, 
											const RHIVertexDeclare& VertexDeclare, const std::vector<RHIShaderMacro>& MacroDefines)
	{
		std::vector< D3D_SHADER_MACRO> D3DShaderMacros;
		ShaderUtil::RHIShaderMarcoToD3DShaderMacro(MacroDefines, D3DShaderMacros);

		bool Ret = ShaderUtil::CompileShader(FileName, D3DShaderMacros.data(), VSMain, "vs_5_0", Code.get_init_ref());
		if (!Ret)
		{
			Assert(false);
			return false;
		}

		std::vector<uint8_t> shaderCode;
		shaderCode.resize(Code->GetBufferSize());
		std::memcpy(&shaderCode[0], Code->GetBufferPointer(), Code->GetBufferSize());
		
		uint32_t NumSamplers = 0;
		uint32_t NumSRVs = 0;
		uint32_t NumCBs = 0;
		uint32_t NumUAVs = 0;
		FShaderCompilerOutput Output;

		ShaderUtil::ExtractParameterMapFromD3DShader(0, shaderCode, NumSamplers, NumSRVs, NumCBs, NumUAVs, Output);

		ResourceCounts.NumCBs = NumCBs;
		ResourceCounts.NumSRVs = NumSRVs;
		ResourceCounts.NumUAVs = NumUAVs;
		ResourceCounts.NumSamplers = NumSamplers;
		ElementDescs = VertexDeclare.GetDeclareDesc();

		std::filesystem::path Path(core::ucs2_u8(FileName));
		KeyName = core::format(Path.filename().string(),"_", VSMain);
		Hash = core::Crc::MemCrc32(KeyName.data(), KeyName.size());
		Hash = core::Crc::HashState(VertexDeclare.GetDeclareDesc().data(), VertexDeclare.GetDeclareDesc().size(), Hash);
		Hash = static_cast<uint32_t>(HashShaderMacros(MacroDefines, Hash));
		return true;
	}

	FD3D12PixelShader::FD3D12PixelShader()
		:RHIPixelShader(SF_Pixel)
	{

	}

	bool FD3D12PixelShader::CreateShader(const std::wstring& FileName, const std::string& PSMain, const std::vector<RHIShaderMacro>& MacroDefines)
	{
		std::vector< D3D_SHADER_MACRO> D3DShaderMacros;
		ShaderUtil::RHIShaderMarcoToD3DShaderMacro(MacroDefines, D3DShaderMacros);

		// ps_5_1: texture arrays / bindless-style indexing (PBR RHI_BINDLESS path); still loads on D3D11 with ps_5_0.
		bool Ret = ShaderUtil::CompileShader(FileName, D3DShaderMacros.data(), PSMain, "ps_5_1", Code.get_init_ref());
		if (!Ret)
		{
			Assert(false);
			return false;
		}
		PSEntryPoint = PSMain;
		std::vector<uint8_t> shaderCode;
		shaderCode.resize(Code->GetBufferSize());
		std::memcpy(&shaderCode[0], Code->GetBufferPointer(), Code->GetBufferSize());

		uint32_t NumSamplers = 0;
		uint32_t NumSRVs = 0;
		uint32_t NumCBs = 0;
		uint32_t NumUAVs = 0;
		FShaderCompilerOutput Output;

		ShaderUtil::ExtractParameterMapFromD3DShader(0, shaderCode, NumSamplers, NumSRVs, NumCBs, NumUAVs, Output);

		ResourceCounts.NumCBs = NumCBs;
		ResourceCounts.NumSRVs = NumSRVs;
		ResourceCounts.NumUAVs = NumUAVs;
		ResourceCounts.NumSamplers = NumSamplers;

		std::filesystem::path Path(core::ucs2_u8(FileName));
		KeyName = core::format(Path.filename().string(), "_", PSMain);
		Hash = core::Crc::MemCrc32(KeyName.data(), KeyName.size());
		Hash = static_cast<uint32_t>(HashShaderMacros(MacroDefines, Hash));
		return true;
	}

	FD3D12ComputeShader::FD3D12ComputeShader()
		:RHIComputeShader(SF_Compute)
	{

	}

	bool FD3D12ComputeShader::CreateShader(const std::wstring& FileName, const std::string& CSMain, const std::vector<RHIShaderMacro>& MacroDefines)
	{
		std::vector< D3D_SHADER_MACRO> D3DShaderMacros;
		ShaderUtil::RHIShaderMarcoToD3DShaderMacro(MacroDefines, D3DShaderMacros);

		bool Ret = ShaderUtil::CompileShader(FileName, D3DShaderMacros.data(), CSMain, "cs_5_0", Code.get_init_ref());
		if (!Ret)
		{
			Assert(false);
			return false;
		}
		CSEntryPoint = CSMain;
		std::vector<uint8_t> shaderCode;
		shaderCode.resize(Code->GetBufferSize());
		std::memcpy(&shaderCode[0], Code->GetBufferPointer(), Code->GetBufferSize());

		uint32_t NumSamplers = 0;
		uint32_t NumSRVs = 0;
		uint32_t NumCBs = 0;
		uint32_t NumUAVs = 0;
		FShaderCompilerOutput Output;

		ShaderUtil::ExtractParameterMapFromD3DShader(0, shaderCode, NumSamplers, NumSRVs, NumCBs, NumUAVs, Output);

		ResourceCounts.NumCBs = NumCBs;
		ResourceCounts.NumSRVs = NumSRVs;
		ResourceCounts.NumUAVs = NumUAVs;
		ResourceCounts.NumSamplers = NumSamplers;

		std::filesystem::path Path(core::ucs2_u8(FileName));
		KeyName = core::format(Path.filename().string(), "_", CSMain);
		Hash = core::Crc::MemCrc32(KeyName.data(), KeyName.size());
		Hash = static_cast<uint32_t>(HashShaderMacros(MacroDefines, Hash));
		return true;
	}

}
