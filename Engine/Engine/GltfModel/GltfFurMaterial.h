#pragma once
#include "GltfModel/GltfMaterial.h"

namespace Engine
{
	class GltfFurMaterial : public GltfMaterial
	{
	public:
		GltfFurMaterial(tinygltf::Model* Model);
		~GltfFurMaterial();
	};
}

