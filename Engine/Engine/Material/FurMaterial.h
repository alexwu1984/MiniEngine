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

		/**
		 * Fur shells: forward pass PS only samples Albedo+Noise (2 textures); the bindless descriptor table setup
		 * costs more than direct slot binding on D3D12 and the shell pass is per-instance hot path, so opt out.
		 * Inner base PBR pass also drops bindless here (shader supports both variants via RHI_BINDLESS macro).
		 */
		bool WantsRHIBindless() const override { return false; }

		const FurConfig& GetFurConfig() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetNoiseTex() const;
	private:
		FurMaterialPrivate* d_ptr = nullptr;
	};
}

