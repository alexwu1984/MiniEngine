#pragma once
#include "core/inc.h"
#include "Engine/Material/GltfMaterial.h"

namespace Engine
{
	struct MaterialRenderP
	{
		std::shared_ptr<GltfMaterial> Material;
	};
}