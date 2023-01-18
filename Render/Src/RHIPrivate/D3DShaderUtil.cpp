#include "RHIPrivate/D3DShaderUtil.h"
#include "core/logger.h"

namespace RenderCore
{

	win32::com_ptr<ID3DBlob> ShaderUtil::CreateShader(const std::wstring& ShaderFile, const std::string& EntryPoint, const std::string& TargetModel)
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
		if (!SUCCEEDED(D3DCompileFromFile(ShaderFile.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, EntryPoint.c_str(), TargetModel.c_str(), compileFlags, 0, ShaderBlob.get_init_ref(), &errors)))
		{
			const char* errStr = (const char*)errors->GetBufferPointer();
			core::err() << "Compile Shader Error:" << errStr;
			errors->Release();
			Assert(false);
		}
		return ShaderBlob;
	}
}

