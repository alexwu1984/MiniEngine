#pragma once
#include "GltfModel/GltfMaterial.h"

namespace Engine
{
	struct GltfFurMaterialPrivate;
	struct GltfFurConfig;
	
	class GltfFurMaterial : public GltfMaterial
	{
	public:
		GltfFurMaterial(GltfModel* Owner, tinygltf::Model* Model);
		~GltfFurMaterial();

		virtual void  InitMaterial(uint32_t MaterialIndex) override;
		virtual MaterialType GetMaterialType() const override;

		const GltfFurConfig& GetFurConfig() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetNoiseTex() const;
	private:
		GltfFurMaterialPrivate* d_ptr = nullptr;
	};
}

