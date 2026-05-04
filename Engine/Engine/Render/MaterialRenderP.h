#pragma once
#include "Engine/Material/GltfMaterial.h"

namespace Engine
{
	struct MaterialRenderP
	{
		std::shared_ptr<GltfMaterial> Material;
	};
}