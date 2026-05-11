#include "D3D12/D3D12Shaders.h"
#include "RHIPrivate/D3DShaderUtil.h"
#include "core/logger.h"
#include "RHIPrivate/ShaderCore.h"
#include "RHIPrivate/ShaderPrecompileUtil.h"
#include "common/crc.h"
#include <d3dcompiler.h>
#include <cstring>
#include <filesystem>
#include <iomanip>
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

	// PBR bindless uses ps_5_1 texture arrays; everything else uses ps_5_0 so D3DReflect reports TextureCube
	// dimensions reliably (ps_5_1/DXIL can mis-report t0 and break GBV #940 under multi-pass descriptor heap churn).
	static bool PixelShaderUsesBindlessMacros(const std::vector<RHIShaderMacro>& MacroDefines)
	{
		for (const RHIShaderMacro& M : MacroDefines)
		{
			if (M.Name == "RHI_BINDLESS")
				return true;
		}
		return false;
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

		std::vector<uint8_t> precompiledBC;
		if (!TryLoadPrecompiledShaderBytecode(FileName, VSMain, "vs_5_0", MacroDefines, precompiledBC))
		{
			bool Ret = ShaderUtil::CompileShader(FileName, D3DShaderMacros.data(), VSMain, "vs_5_0", Code.get_init_ref());
			if (!Ret)
			{
				core::err() << "[Shader] vertex shader JIT compile failed entry=" << VSMain << " file=" << FileName;
				Assert(false);
				return false;
			}
		}
		else
		{
			HRESULT hrBlob = D3DCreateBlob(precompiledBC.size(), Code.get_init_ref());
			if (FAILED(hrBlob) || !Code.is_valid())
			{
				core::err() << "[Shader] D3DCreateBlob failed for precompiled vertex shader hr=0x" << std::hex << std::uppercase
							<< static_cast<unsigned long>(hrBlob) << std::dec << " bytes=" << precompiledBC.size()
							<< " entry=" << VSMain << " file=" << FileName;
				Assert(false);
				return false;
			}
			std::memcpy(Code->GetBufferPointer(), precompiledBC.data(), precompiledBC.size());
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

		ResourceCounts = Output.ResourceCounts;
		CBBind0RootConstantsDwords = ShaderUtil::GetCBBindPoint0RootConstantDwordCount(0, shaderCode);
		ElementDescs = VertexDeclare.GetDeclareDesc();

		std::filesystem::path Path(core::ucs2_u8(FileName));
		KeyName = core::format(Path.filename().string(),"_", VSMain);
		Hash = core::Crc::MemCrc32(KeyName.data(), KeyName.size());
		Hash = core::Crc::HashState(VertexDeclare.GetDeclareDesc().data(), VertexDeclare.GetDeclareDesc().size(), Hash);
		Hash = static_cast<uint32_t>(HashShaderMacros(MacroDefines, Hash));
		Hash = core::Crc::MemCrc32(shaderCode.data(), (int32_t)shaderCode.size(), Hash);
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

		const char* const psTarget = PixelShaderUsesBindlessMacros(MacroDefines) ? "ps_5_1" : "ps_5_0";

		std::vector<uint8_t> precompiledBC;
		TryLoadPrecompiledShaderBytecode(FileName, PSMain, psTarget, MacroDefines, precompiledBC);
		if (!precompiledBC.empty())
		{
			HRESULT hrBlob = D3DCreateBlob(precompiledBC.size(), Code.get_init_ref());
			if (FAILED(hrBlob) || !Code.is_valid())
			{
				core::err() << "[Shader] D3DCreateBlob failed for precompiled pixel shader hr=0x" << std::hex << std::uppercase
							<< static_cast<unsigned long>(hrBlob) << std::dec << " bytes=" << precompiledBC.size()
							<< " entry=" << PSMain << " target=" << psTarget << " file=" << FileName;
				Assert(false);
				return false;
			}
			std::memcpy(Code->GetBufferPointer(), precompiledBC.data(), precompiledBC.size());
		}
		else
		{
			bool Ret = ShaderUtil::CompileShader(FileName, D3DShaderMacros.data(), PSMain, psTarget, Code.get_init_ref());
			if (!Ret)
			{
				core::err() << "[Shader] pixel shader JIT compile failed entry=" << PSMain << " target=" << psTarget << " file=" << FileName;
				Assert(false);
				return false;
			}
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

		ResourceCounts = Output.ResourceCounts;
		CBBind0RootConstantsDwords = ShaderUtil::GetCBBindPoint0RootConstantDwordCount(0, shaderCode);

		std::filesystem::path Path(core::ucs2_u8(FileName));
		KeyName = core::format(Path.filename().string(), "_", PSMain);
		Hash = core::Crc::MemCrc32(KeyName.data(), KeyName.size());
		Hash = static_cast<uint32_t>(HashShaderMacros(MacroDefines, Hash));
		Hash = core::Crc::MemCrc32(shaderCode.data(), (int32_t)shaderCode.size(), Hash);
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

		static constexpr char kCsProfile[] = "cs_5_0";
		std::vector<uint8_t> precompiledBC;
		if (TryLoadPrecompiledShaderBytecode(FileName, CSMain, kCsProfile, MacroDefines, precompiledBC))
		{
			HRESULT hrBlob = D3DCreateBlob(precompiledBC.size(), Code.get_init_ref());
			if (FAILED(hrBlob) || !Code.is_valid())
			{
				core::err() << "[Shader] D3DCreateBlob failed for precompiled compute shader hr=0x" << std::hex << std::uppercase
							<< static_cast<unsigned long>(hrBlob) << std::dec << " bytes=" << precompiledBC.size()
							<< " entry=" << CSMain << " file=" << FileName;
				Assert(false);
				return false;
			}
			std::memcpy(Code->GetBufferPointer(), precompiledBC.data(), precompiledBC.size());
		}
		else
		{
			bool Ret = ShaderUtil::CompileShader(FileName, D3DShaderMacros.data(), CSMain, kCsProfile, Code.get_init_ref());
			if (!Ret)
			{
				core::err() << "[Shader] compute shader JIT compile failed entry=" << CSMain << " file=" << FileName;
				Assert(false);
				return false;
			}
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

		ResourceCounts = Output.ResourceCounts;
		CBBind0RootConstantsDwords = ShaderUtil::GetCBBindPoint0RootConstantDwordCount(0, shaderCode);

		std::filesystem::path Path(core::ucs2_u8(FileName));
		KeyName = core::format(Path.filename().string(), "_", CSMain);
		Hash = core::Crc::MemCrc32(KeyName.data(), KeyName.size());
		Hash = static_cast<uint32_t>(HashShaderMacros(MacroDefines, Hash));
		Hash = core::Crc::MemCrc32(shaderCode.data(), (int32_t)shaderCode.size(), Hash);
		return true;
	}

}
