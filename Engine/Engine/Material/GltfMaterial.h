#pragma once
#include "Material/MaterialBase.h"
#include "tinygltf/tiny_gltf.h"

namespace RenderCore
{
	class RHITexture2D;
}

namespace Engine
{
	struct GltfMaterialPrivate;
	class GltfModel;
	struct MaterialConfig;

	class GltfMaterial : public MaterialBase
	{
	public:
		GltfMaterial(GltfModel* Owner,tinygltf::Model* Model);
		~GltfMaterial();

		virtual void  InitMaterial(uint32_t MaterialIndex);

		std::string GetMaterialName() const override;
		bool IsTransparent() const override;
		bool WritesTranslucentDepthToSceneBuffer() const override;
		bool UsesMaterialAlphaMask() const override;
		float GetMaterialAlphaCutoff() const override;
		void SetTransparent(bool Transparent);

		MaterialType GetMaterialType() const override;

		std::shared_ptr<RenderCore::RHITexture2D> GetBaseColorTexture() const override;
		std::shared_ptr<RenderCore::RHITexture2D> GetMetallicRoughnessTexture() const override;
		std::shared_ptr<RenderCore::RHITexture2D> GetNormalTexture() const override;
		std::shared_ptr<RenderCore::RHITexture2D> GetEmissiveTexture() const override;
		std::shared_ptr<RenderCore::RHITexture2D> GetOcclusionTexture() const override;

		const MaterialConfig& GetMaterialConfig() const override;

	protected:
		tinygltf::Model* GetModel();
		GltfModel* GetOwner();

	private:
		GltfMaterialPrivate* d_ptr = nullptr;
	};
}