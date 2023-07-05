#pragma once
#include "GltfModel/GltfMaterial.h"

namespace Engine
{
	struct GltfFurMaterialPrivate;
	
	class GltfFurMaterial : public GltfMaterial
	{
	public:
		GltfFurMaterial(GltfModel* Owner, tinygltf::Model* Model);
		~GltfFurMaterial();

		virtual void  InitMaterial(uint32_t MaterialIndex) override;
		virtual MaterialType GetMaterialType() const override;
	private:
		GltfFurMaterialPrivate* d_ptr = nullptr;
	};
}

