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

	GltfFurMaterial::GltfFurMaterial(GltfModel* Owner, tinygltf::Model* Model)
		:GltfMaterial(Owner,Model)
		,d_ptr(new GltfFurMaterialPrivate())
	{

	}

	GltfFurMaterial::~GltfFurMaterial()
	{
		delete d_ptr;
	}

	void GltfFurMaterial::InitMaterial(uint32_t MaterialIndex)
	{
		GltfMaterial::InitMaterial(MaterialIndex);
		SetTransparent(true);
	}

	GltfMaterial::MaterialType GltfFurMaterial::GetMaterialType() const
	{
		return MaterialType::FUR;
	}

}
