#pragma once
#include "core/inc.h"
#include "Engine/GltfModel/GltfMaterial.h"

namespace Engine
{
	struct MaterialRenderP
	{
		std::shared_ptr<GltfMaterial> Material;
	};
}