#include "GltfModel/GltfFurMaterial.h"

namespace Engine
{
	struct GltfFurMaterialPrivate
	{
		std::shared_ptr<RenderCore::RHITexture2D> NoiseTex ;
		int NumLayers = 30;
		float FurLength = 0.1f;
		float UVScale = 20.0f;
	};

	GltfFurMaterial::GltfFurMaterial(tinygltf::Model* Model)
		:GltfMaterial(Model)
		,d_ptr(new GltfFurMaterialPrivate())
	{

	}

	GltfFurMaterial::~GltfFurMaterial()
	{
		delete d_ptr;
	}

}
