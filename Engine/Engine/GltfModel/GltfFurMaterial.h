#pragma once
#include "GltfModel/GltfMaterial.h"

namespace Engine
{
	struct GltfFurMaterialPrivate;
	
	class GltfFurMaterial : public GltfMaterial
	{
	public:
		GltfFurMaterial(tinygltf::Model* Model);
		~GltfFurMaterial();

	private:
		GltfFurMaterialPrivate* d_ptr = nullptr;
	};
}

