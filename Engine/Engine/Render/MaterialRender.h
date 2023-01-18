#pragma once
#include "core/inc.h"
#include "tinygltf/json.h"

namespace Engine
{
	struct MaterialRenderP;
	class GltfMaterial;

	class MaterialRender
	{
	public:
		MaterialRender(std::shared_ptr<GltfMaterial> Material);
		virtual ~MaterialRender();
		
		virtual void InitRenderResource(nlohmann::json& jsonObj);
		virtual void InitShader(const std::string& Path) = 0;

	protected:
		std::shared_ptr< MaterialRenderP> MaterialRenderData;
	};
}