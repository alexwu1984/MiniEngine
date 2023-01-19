#pragma once
#include "core/inc.h"
#include "tinygltf/json.h"

namespace Engine
{

	class MaterialRender
	{
	public:
		MaterialRender() = default;
		virtual ~MaterialRender();
		
		virtual void InitRenderResource(nlohmann::json& jsonObj) = 0;
		virtual void InitShader(const std::wstring& Path) = 0;
	};
}