#include "RHIPrivate/D3DShaderUtil.h"
#include "RHI/RHIShdader.h"
#include "RHIPrivate/ShaderCore.h"
#include "D3D12/D3D12Limits.h"
#include "core/commandline.h"
#include "core/logger.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <Shaders/dxc/dxcapi.h>
#include <Shaders/dxc/Support/dxcapi.use.h>

namespace RenderCore
{
	UINT ShaderUtil::GetD3DCompileFlagsForBuild()
	{
#if defined(DEBUG) || defined(_DEBUG)
		static bool s_loggedOnce = false;
		if (!s_loggedOnce)
		{
			s_loggedOnce = true;
			if (core::CommandLine::Get().GetSwitch("shaderdebug"))
				core::inf() << "Shader compile flags: DEBUG + SKIP_OPTIMIZATION (shaderdebug=1); GPU will be slower, better for PIX/shader breakpoints.";
			else
				core::inf() << "Shader compile flags: default optimized in Debug builds; pass shaderdebug=1 for PIX-friendly unoptimized shaders.";
		}
		if (core::CommandLine::Get().GetSwitch("shaderdebug"))
			return D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
		return 0;
#else
		return 0;
#endif
	}

	namespace
	{
		void LogShaderCompileMs(const std::wstring& file, const std::string& entry, const std::string& target, bool ok,
			std::chrono::steady_clock::time_point start)
		{
			const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
			core::inf() << "[ShaderCompile] " << file << " entry=" << entry << " target=" << target << " ok=" << ok << " timeMs=" << ms;
		}

		void LogD3DCompileFromFileFailure(const std::wstring& file, const std::string& entry, const std::string& target,
			HRESULT hr, ID3DBlob* errors)
		{
			core::err() << "[ShaderCompile] FAILED " << file << " entry=" << entry << " target=" << target << " hr=0x"
						<< std::hex << std::uppercase << static_cast<unsigned long>(hr) << std::dec;
			if (errors && errors->GetBufferPointer() && errors->GetBufferSize() > 0)
				core::err() << static_cast<const char*>(errors->GetBufferPointer());
			else
				core::err() << " (no D3D compiler error text)";
		}
	} // namespace

	#define VERIFYHRESULT(expr) { HRESULT HR##__LINE__ = expr; if (FAILED(HR##__LINE__)) { /*UE_LOG(LogD3D11ShaderCompiler, Fatal, TEXT(#expr " failed: Result=%08x"), HR##__LINE__);*/ } }

	#ifndef DXIL_FOURCC
	#define DXIL_FOURCC(ch0, ch1, ch2, ch3) (                            \
	  (uint32_t)(uint8_t)(ch0)        | (uint32_t)(uint8_t)(ch1) << 8  | \
	  (uint32_t)(uint8_t)(ch2) << 16  | (uint32_t)(uint8_t)(ch3) << 24   \
	  )
	#endif

	static dxc::DxcDllSupport& GetDxcDllHelper()
	{
		static dxc::DxcDllSupport DxcDllSupport;
		static bool DxcDllInitialized = false;
		if (!DxcDllInitialized)
		{
			VERIFYHRESULT(DxcDllSupport.Initialize());
			DxcDllInitialized = true;
		}
		return DxcDllSupport;
	}

	win32::com_ptr<ID3DBlob> ShaderUtil::CreateShader(const std::wstring& ShaderFile, const std::string& EntryPoint,
		const std::string& TargetModel, const D3D_SHADER_MACRO* pDefines)
	{
		// Declare handles
		win32::com_ptr<ID3DBlob> errors;

		const UINT compileFlags = ShaderUtil::GetD3DCompileFlagsForBuild();

		win32::com_ptr<ID3DBlob> ShaderBlob;
		const auto compileStart = std::chrono::steady_clock::now();
		HRESULT HR = D3DCompileFromFile(ShaderFile.c_str(), pDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, EntryPoint.c_str(),
			TargetModel.c_str(), compileFlags, 0, ShaderBlob.get_init_ref(), errors.get_init_ref());
		LogShaderCompileMs(ShaderFile, EntryPoint, TargetModel, SUCCEEDED(HR), compileStart);
		if (FAILED(HR))
		{
			LogD3DCompileFromFileFailure(ShaderFile, EntryPoint, TargetModel, HR, errors.get());
			return {};
		}
		return ShaderBlob;
	}

	void ShaderUtil::RHIShaderMarcoToD3DShaderMacro(const std::vector<RHIShaderMacro>& RHIShaderMacros, std::vector<D3D_SHADER_MACRO>& D3DShaderMacros)
	{
		D3DShaderMacros.resize(RHIShaderMacros.size() + 1);
		for (size_t index = 0; index < RHIShaderMacros.size(); ++index)
		{
			D3DShaderMacros[index] = { RHIShaderMacros[index].Name.c_str(),RHIShaderMacros[index].Definition.c_str() };
		}
		D3DShaderMacros[RHIShaderMacros.size()] = { nullptr,nullptr };
	}

	HRESULT ShaderUtil::D3DCreateReflectionFromBlob(ID3DBlob* DxilBlob, win32::com_ptr<ID3D12ShaderReflection>& OutReflection)
	{
		dxc::DxcDllSupport& DxcDllHelper = GetDxcDllHelper();

		win32::com_ptr<IDxcContainerReflection> ContainerReflection;
		VERIFYHRESULT(DxcDllHelper.CreateInstance(CLSID_DxcContainerReflection, ContainerReflection.get_init_ref()));
		VERIFYHRESULT(ContainerReflection->Load((IDxcBlob*)DxilBlob));

		const uint32_t DxilPartKind = DXIL_FOURCC('D', 'X', 'I', 'L');
		uint32_t DxilPartIndex = ~0u;
		VERIFYHRESULT(ContainerReflection->FindFirstPartKind(DxilPartKind, &DxilPartIndex));

		HRESULT Result = ContainerReflection->GetPartReflection(DxilPartIndex, IID_PPV_ARGS(OutReflection.get_init_ref()));

		return Result;
	}

#undef VERIFYHRESULT

	inline bool IsCompatibleBinding(const D3D12_SHADER_INPUT_BIND_DESC& BindDesc, uint32_t BindingSpace)
	{
		return BindDesc.Space == BindingSpace;
	}

	uint8_t ShaderUtil::GetCBBindPoint0RootConstantDwordCount(uint32_t BindingSpace, const std::vector<uint8_t>& Code)
	{
		win32::com_ptr<ID3D12ShaderReflection> Reflector;
		if (FAILED(::D3DReflect(Code.data(), Code.size(), IID_PPV_ARGS(Reflector.get_init_ref()))))
			return 0;

		D3D12_SHADER_DESC ShaderDesc{};
		Reflector->GetDesc(&ShaderDesc);

		for (uint32_t ResourceIndex = 0; ResourceIndex < ShaderDesc.BoundResources; ++ResourceIndex)
		{
			D3D12_SHADER_INPUT_BIND_DESC BindDesc{};
			Reflector->GetResourceBindingDesc(ResourceIndex, &BindDesc);
			if (!IsCompatibleBinding(BindDesc, BindingSpace))
				continue;
			if (BindDesc.Type != D3D10_SIT_CBUFFER && BindDesc.Type != D3D10_SIT_TBUFFER)
				continue;
			if (BindDesc.BindPoint != 0)
				continue;

			ID3D12ShaderReflectionConstantBuffer* ConstantBuffer = Reflector->GetConstantBufferByName(BindDesc.Name);
			if (!ConstantBuffer)
				return 0;

			D3D12_SHADER_BUFFER_DESC CBDesc{};
			ConstantBuffer->GetDesc(&CBDesc);
			if (std::strcmp(CBDesc.Name, "$Globals") == 0)
				return 0;

			const uint32_t sizeBytes = CBDesc.Size;
			if (sizeBytes == 0 || sizeBytes > 256)
				return 0;

			const uint32_t paddedBytes = (sizeBytes + 15u) & ~15u;
			const uint32_t dwords = paddedBytes / 4u;
			if (dwords == 0 || dwords > 64)
				return 0;

			return static_cast<uint8_t>(dwords);
		}

		return 0;
	}

	void ShaderUtil::ExtractParameterMapFromD3DShader(uint32_t BindingSpace, const std::vector<uint8_t>& Code, uint32_t& NumSamplers, 
		uint32_t& NumSRVs, uint32_t& NumCBs, uint32_t& NumUAVs, FShaderCompilerOutput& Output)
	{
		win32::com_ptr<ID3D12ShaderReflection> Reflector;

		HRESULT hr = ::D3DReflect(Code.data(), Code.size(),
			IID_PPV_ARGS(Reflector.get_init_ref()));

		if (SUCCEEDED(hr))
		{
			// SRV register layout and per-slot null-view dimensions come from bytecode reflection (BindDesc.Dimension).
			std::memset(Output.ResourceCounts.SrvSlotNullViewDimension, (int)D3D_SRV_DIMENSION_TEXTURE2D, sizeof(Output.ResourceCounts.SrvSlotNullViewDimension));

			D3D12_SHADER_DESC ShaderDesc = {};
			Reflector->GetDesc(&ShaderDesc);

			for (uint32_t ResourceIndex = 0; ResourceIndex < ShaderDesc.BoundResources; ResourceIndex++)
			{
				D3D12_SHADER_INPUT_BIND_DESC BindDesc;
				Reflector->GetResourceBindingDesc(ResourceIndex, &BindDesc);

				if (!IsCompatibleBinding(BindDesc, BindingSpace))
				{
					continue;
				}

				if (BindDesc.Type == D3D10_SIT_CBUFFER || BindDesc.Type == D3D10_SIT_TBUFFER)
				{
					const uint32_t CBIndex = BindDesc.BindPoint;
					ID3D12ShaderReflectionConstantBuffer* ConstantBuffer = Reflector->GetConstantBufferByName(BindDesc.Name);
					D3D12_SHADER_BUFFER_DESC CBDesc;
					ConstantBuffer->GetDesc(&CBDesc);
					bool bGlobalCB = (strcmp(CBDesc.Name, "$Globals") == 0);

					if (bGlobalCB)
					{
						// Track all of the variables in this constant buffer.
						for (uint32_t ConstantIndex = 0; ConstantIndex < CBDesc.Variables; ConstantIndex++)
						{
							ID3D12ShaderReflectionVariable* Variable = ConstantBuffer->GetVariableByIndex(ConstantIndex);
							D3D12_SHADER_VARIABLE_DESC VariableDesc;
							Variable->GetDesc(&VariableDesc);
							if (VariableDesc.uFlags & D3D10_SVF_USED)
							{

								Output.ParameterMap.AddParameterAllocation(
									VariableDesc.Name,
									CBIndex,
									VariableDesc.StartOffset,
									VariableDesc.Size,
									EShaderParameterType::LooseData
								);
							}
						}
					}
					else
					{
						// Track just the constant buffer itself.
						Output.ParameterMap.AddParameterAllocation(
							CBDesc.Name,
							CBIndex,
							0,
							0,
							EShaderParameterType::UniformBuffer
						);
					}

					NumCBs = math::Max(NumCBs, BindDesc.BindPoint + BindDesc.BindCount);
				}
				else if (BindDesc.Type == D3D10_SIT_TEXTURE || BindDesc.Type == D3D10_SIT_SAMPLER)
				{
					// BindCount can be >1 for SM5.1+ texture arrays (e.g. PBR bindless material slot batch).
					const uint32_t BindCount = (BindDesc.BindCount > 0u) ? BindDesc.BindCount : 1u;
					EShaderParameterType ParameterType = EShaderParameterType::Num;
					if (BindDesc.Type == D3D10_SIT_SAMPLER)
					{
						ParameterType = EShaderParameterType::Sampler;
						NumSamplers = math::Max(NumSamplers, BindDesc.BindPoint + BindCount);
					}
					else if (BindDesc.Type == D3D10_SIT_TEXTURE)
					{
						ParameterType = EShaderParameterType::SRV;
						NumSRVs = math::Max(NumSRVs, BindDesc.BindPoint + BindCount);

						const uint8_t dimByte = static_cast<uint8_t>(BindDesc.Dimension);
						for (uint32_t s = 0; s < BindCount; ++s)
						{
							const uint32_t slot = BindDesc.BindPoint + s;
							if (slot < kEngineSrvSlotNullDimensionCount)
								Output.ResourceCounts.SrvSlotNullViewDimension[slot] = dimByte;
						}
					}

					// Add a parameter for the texture only, the sampler index will be invalid
					Output.ParameterMap.AddParameterAllocation(
						BindDesc.Name,
						0,
						BindDesc.BindPoint,
						BindCount,
						ParameterType
					);
				}
				else if (BindDesc.Type == D3D11_SIT_UAV_RWTYPED || BindDesc.Type == D3D11_SIT_UAV_RWSTRUCTURED ||
					BindDesc.Type == D3D11_SIT_UAV_RWBYTEADDRESS || BindDesc.Type == D3D11_SIT_UAV_RWSTRUCTURED_WITH_COUNTER ||
					BindDesc.Type == D3D11_SIT_UAV_APPEND_STRUCTURED)
				{
					assert(BindDesc.BindCount == 1);

					const uint32_t BindCount = 1;
					Output.ParameterMap.AddParameterAllocation(
						BindDesc.Name,
						0,
						BindDesc.BindPoint,
						BindCount,
						EShaderParameterType::UAV
					);

					NumUAVs = math::Max(NumUAVs, BindDesc.BindPoint + BindCount);
				}
				else if (BindDesc.Type == D3D11_SIT_STRUCTURED || BindDesc.Type == D3D11_SIT_BYTEADDRESS)
				{
					assert(BindDesc.BindCount == 1);

					const uint32_t BindCount = 1;
					Output.ParameterMap.AddParameterAllocation(
						BindDesc.Name,
						0,
						BindDesc.BindPoint,
						BindCount,
						EShaderParameterType::SRV
					);

					NumSRVs = math::Max(NumSRVs, BindDesc.BindPoint + BindCount);
					if (BindDesc.BindPoint < kEngineSrvSlotNullDimensionCount)
						Output.ResourceCounts.SrvSlotNullViewDimension[BindDesc.BindPoint] = static_cast<uint8_t>(BindDesc.Dimension);
				}
				// #dxr_todo: D3D_SIT_RTACCELERATIONSTRUCTURE is declared in latest version of dxcapi.h. Update this code after upgrading DXC.
				else if (BindDesc.Type == 12 /*D3D_SIT_RTACCELERATIONSTRUCTURE*/)
				{
					// Acceleration structure resources are treated as SRVs.
					assert(BindDesc.BindCount == 1);

					const uint32_t BindCount = 1;
					Output.ParameterMap.AddParameterAllocation(
						BindDesc.Name,
						0,
						BindDesc.BindPoint,
						BindCount,
						EShaderParameterType::SRV
					);

					NumSRVs = math::Max(NumSRVs, BindDesc.BindPoint + BindCount);
					if (BindDesc.BindPoint < kEngineSrvSlotNullDimensionCount)
						Output.ResourceCounts.SrvSlotNullViewDimension[BindDesc.BindPoint] = static_cast<uint8_t>(BindDesc.Dimension);
				}
			}

			Output.ResourceCounts.NumSamplers = static_cast<uint8_t>(std::min(NumSamplers, static_cast<uint32_t>(MAX_SAMPLERS)));
			Output.ResourceCounts.NumSRVs = static_cast<uint8_t>(std::min(NumSRVs, static_cast<uint32_t>(MAX_SRVS)));
			Output.ResourceCounts.NumCBs = static_cast<uint8_t>(std::min(NumCBs, static_cast<uint32_t>(MAX_CBS)));
			Output.ResourceCounts.NumUAVs = static_cast<uint8_t>(std::min(NumUAVs, static_cast<uint32_t>(MAX_UAVS)));
		}
		else
		{
			core::err() << "[ShaderReflect] D3DReflect failed hr=0x" << std::hex << std::uppercase << static_cast<unsigned long>(hr)
						<< std::dec << " bytecodeBytes=" << Code.size()
						<< " (root signature / bindings will be wrong - check shader bytecode)";
		}
	}

	std::vector<uint8_t> ShaderUtil::CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target)
	{
		const UINT compileFlags = ShaderUtil::GetD3DCompileFlagsForBuild();

		HRESULT hr = S_OK;

		win32::com_ptr<ID3DBlob> Shader;
		win32::com_ptr<ID3DBlob> errors;
		const auto compileStart = std::chrono::steady_clock::now();
		hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entrypoint.c_str(), target.c_str(), compileFlags, 0, Shader.get_init_ref(), errors.get_init_ref());
		LogShaderCompileMs(filename, entrypoint, target, hr == S_OK, compileStart);

		if (errors != nullptr)
			OutputDebugStringA((char*)errors->GetBufferPointer());

		std::vector<uint8_t> shaderCode;
		if (hr == S_OK)
		{
			shaderCode.resize(Shader->GetBufferSize());
			std::memcpy(&shaderCode[0], Shader->GetBufferPointer(), Shader->GetBufferSize());
		}
		else
			LogD3DCompileFromFileFailure(filename, entrypoint, target, hr, errors.get());
		return shaderCode;
	}

	bool ShaderUtil::CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target, ID3DBlob** ppShader)
	{
		const UINT compileFlags = ShaderUtil::GetD3DCompileFlagsForBuild();

		HRESULT hr = S_OK;
		win32::com_ptr<ID3DBlob> errors;
		const auto compileStart = std::chrono::steady_clock::now();
		hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entrypoint.c_str(), target.c_str(), compileFlags, 0, ppShader, errors.get_init_ref());
		LogShaderCompileMs(filename, entrypoint, target, SUCCEEDED(hr), compileStart);

		if (errors != nullptr)
			OutputDebugStringA((char*)errors->GetBufferPointer());

		if (FAILED(hr))
		{
			LogD3DCompileFromFileFailure(filename, entrypoint, target, hr, errors.get());
			return false;
		}
		if (!ppShader || !*ppShader)
		{
			core::err() << "[ShaderCompile] succeeded HRESULT but null shader blob " << filename << " entry=" << entrypoint << " target=" << target;
			return false;
		}
		return true;
	}

}

