#pragma once
#include "Material/GltfMaterial.h"
#include "Scene/SceneModelSettings.h"

namespace Engine
{
	struct FurMaterialPrivate;
	
	class FurMaterial : public GltfMaterial
	{
	public:
		FurMaterial(GltfModel* Owner, tinygltf::Model* Model);
		~FurMaterial();

		virtual void  InitMaterial(uint32_t MaterialIndex) override;
		virtual MaterialType GetMaterialType() const override;

		const FurConfig& GetFurConfig() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetNoiseTex() const;
	private:
		FurMaterialPrivate* d_ptr = nullptr;
	};
}

