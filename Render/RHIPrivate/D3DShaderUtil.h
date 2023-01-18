#pragma once
#include "core/inc.h"
#include "win/com_ptr.h"
#include <D3Dcompiler.h>

namespace RenderCore
{
	class ShaderUtil
	{
	public:
		static win32::com_ptr<ID3DBlob> CreateShader(const std::wstring& ShaderFile, const std::string& EntryPoint, const std::string& TargetModel);
	};
	
}