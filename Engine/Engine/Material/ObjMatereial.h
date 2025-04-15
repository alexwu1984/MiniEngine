#pragma once
#include "Material/MaterialBase.h"
#include <Assimp/material.h>

struct aiMesh;
struct aiScene;

namespace Engine
{
	struct ObjMaterialPrivate;

	class ObjMaterial : public MaterialBase
	{
	public:
		ObjMaterial(const aiScene* pScene, aiMesh* pMesh, const std::string& Directory);
		~ObjMaterial();
		void Init();
		virtual MaterialType GetMaterialType() const override{
			return MaterialType::PBR;
		};
		virtual std::string GetMaterialName() const override;
		virtual bool IsTransparent() const override;
		virtual std::shared_ptr<RenderCore::RHITexture2D> GetBaseColorTexture() const override;
		virtual std::shared_ptr<RenderCore::RHITexture2D> GetMetallicRoughnessTexture() const override;
		virtual std::shared_ptr<RenderCore::RHITexture2D> GetNormalTexture() const override;
		virtual std::shared_ptr<RenderCore::RHITexture2D> GetEmissiveTexture() const override;
		virtual std::shared_ptr<RenderCore::RHITexture2D> GetOcclusionTexture() const override;
		virtual const MaterialConfig& GetMaterialConfig() const override;
	private:
		void loadTextureFromMaterial(aiTextureType vTextureType, const aiMaterial* vMat, std::map< int32_t, aiString> &TexNames);
	private:
		ObjMaterialPrivate* d_ptr = nullptr;
	};
}