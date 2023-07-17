#pragma once
#include "core/inc.h"
#include "tinygltf/tiny_gltf.h"

namespace RenderCore
{
	class RHITexture2D;
}

namespace Engine
{
	struct GltfMaterialPrivate;
	class GltfModel;

	class GltfMaterial 
	{
	public:
		enum class MaterialType: uint8_t
		{
			PBR,
			FUR,
		};

	public:
		GltfMaterial(GltfModel* Owner,tinygltf::Model* Model);
		~GltfMaterial();

		virtual void  InitMaterial(uint32_t MaterialIndex);
		std::string GetMaterialName() const;
		bool IsTransparent() const;
		void SetTransparent(bool Transparent);

		virtual MaterialType GetMaterialType() const;

		std::shared_ptr<RenderCore::RHITexture2D> GetBaseColorTexture() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetMetallicRoughnessTexture() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetNormalTexture() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetEmissiveTexture() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetOcclusionTexture() const;

	protected:
		tinygltf::Model* GetModel();
		GltfModel* GetOwner();

	private:
		GltfMaterialPrivate* d_ptr = nullptr;
	};
}