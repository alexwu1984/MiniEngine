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

	protected:
		std::shared_ptr< MaterialRenderP> MaterialRenderData;
	};
}