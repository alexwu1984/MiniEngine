#include "RHIPrivate/D3DShaderUtil.h"
#include "RHI/RHIShdader.h"
#include "core/logger.h"

namespace RenderCore
{

	win32::com_ptr<ID3DBlob> ShaderUtil::CreateShader(const std::wstring& ShaderFile, const std::string& EntryPoint, const std::string& TargetModel, const D3D_SHADER_MACRO* pDefines)
	{
		// Declare handles
		ID3DBlob* errors = nullptr;

#ifdef _DEBUG
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		win32::com_ptr<ID3DBlob> ShaderBlob;
		if (!SUCCEEDED(D3DCompileFromFile(ShaderFile.c_str(), pDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, EntryPoint.c_str(), TargetModel.c_str(), compileFlags, 0, ShaderBlob.get_init_ref(), &errors)))
		{
			const char* errStr = (const char*)errors->GetBufferPointer();
			core::err() << "Compile Shader Error:" << errStr;
			errors->Release();
			Assert(false);
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

}

