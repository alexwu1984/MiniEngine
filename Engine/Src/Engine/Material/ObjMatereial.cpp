#include "Material/ObjMatereial.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "RHI/RHITexture2D.h"
#include "Thread/RenderThread.h"
#include "GltfModel/GltfModelConfig.h"
#include <Assimp/Importer.hpp>
#include <Assimp/scene.h>
#include <Assimp/postprocess.h>

namespace Engine
{
	using namespace RenderCore;
	using namespace math;

#define MESH_ID			"u_MeshId"
#define SHININESS		"u_Shininess"
#define REFRACTI		"u_Refracti"
#define AMBIENT_COLOR	"u_AmbientColor"
#define DIFFUSE_COLOR	"u_DiffuseColor"
#define SPECULAR_COLOR	"u_SpecularColor"
#define DIFFUSE_TEX		"u_DiffuseTexture"
#define SPECULAR_TEX	"u_SpecularTexture"
#define NORMAL_TEX		"u_NormalTexture"
#define ROUGHNESS_TEX	"u_RoughnessTexture"
#define METALLIC_TEX	"u_MetallicTexture"

	struct SMeshMatProperties
	{
		math::Vector3 AmbientColor;
		math::Vector3 DiffuseColor;
		math::Vector3 SpecularColor;
		float Shininess = 0.0f;
		float Refracti = 0.0f;
	};

	struct ObjMaterialPrivate
	{
		aiMesh* vAiMesh = nullptr;
		const aiScene* Scene = nullptr;
		std::string MaterialName;
		std::string Directory;
		bool DoubleSided = false;
		bool IsTransParent = false;

		std::shared_ptr<RHITexture2D> BaseColorTexture;
		std::shared_ptr<RHITexture2D> MetallicRoughnessTexture;
		std::shared_ptr<RHITexture2D> NormalTexture;
		std::shared_ptr<RHITexture2D> EmissiveTexture;
		std::shared_ptr<RHITexture2D> OcclusionTexture;

		MaterialConfig Config;
		SMeshMatProperties MatProperty;
	};

	ObjMaterial::ObjMaterial(const aiScene* pScene, aiMesh* pMesh, const std::string& Directory)
		:d_ptr(new ObjMaterialPrivate())
	{
		C_P(ObjMaterial);
		d->Scene = pScene;
		d->vAiMesh = pMesh;
		d->Directory = Directory;
	}

	ObjMaterial::~ObjMaterial()
	{
		delete d_ptr;
	}

	void ObjMaterial::Init()
	{
		C_P(ObjMaterial);
		if (d->vAiMesh->mMaterialIndex < 0)
			return;
		
		aiMaterial* pAiMat = d->Scene->mMaterials[d->vAiMesh->mMaterialIndex];
		std::map< int32_t, aiString> TexNames;
		loadTextureFromMaterial(aiTextureType_DIFFUSE, pAiMat, TexNames);
		loadTextureFromMaterial(aiTextureType_SHININESS, pAiMat, TexNames);
		loadTextureFromMaterial(aiTextureType_HEIGHT, pAiMat, TexNames);
		loadTextureFromMaterial(aiTextureType_EMISSIVE, pAiMat, TexNames);
		loadTextureFromMaterial(aiTextureType_LIGHTMAP, pAiMat, TexNames);

		aiColor3D AmbientColor, DiffuseColor, SpecularColor;
		float Shininess = 0.0f, Refracti = 0.0f;
		pAiMat->Get(AI_MATKEY_COLOR_AMBIENT, AmbientColor);
		pAiMat->Get(AI_MATKEY_COLOR_DIFFUSE, DiffuseColor);
		pAiMat->Get(AI_MATKEY_COLOR_SPECULAR, SpecularColor);
		pAiMat->Get(AI_MATKEY_SHININESS, Shininess);
		pAiMat->Get(AI_MATKEY_REFRACTI, Refracti);
		d->MatProperty.AmbientColor = { AmbientColor.r, AmbientColor.g, AmbientColor.b };
		d->MatProperty.DiffuseColor = { DiffuseColor.r, DiffuseColor.g, DiffuseColor.b };
		d->MatProperty.SpecularColor = { SpecularColor.r, SpecularColor.g, SpecularColor.b };
		d->MatProperty.Shininess = Shininess;
		d->MatProperty.Refracti = Refracti;

		auto CreateTexture = [this](const aiString& Str, const core::FLinearColor& Color) {
			C_P(ObjMaterial);
			std::shared_ptr<RHITexture2D> TexRHI;
			if (Str.length > 0)
			{
				std::string TexPath = d->Directory + "/" + Str.C_Str();
				TexRHI = GEngine->GetRHI()->RHICreateTexture2D(core::u8_ucs2(TexPath));
			}
			else
			{
				TexRHI = GEngine->GetRHI()->RHICreateTexture2D(Color);
			}

			return TexRHI;
		};

		auto CreateTexCommand = [this, CreateTexture, TexNames](DynamicRHI* DyRHI) {
			C_P(ObjMaterial);
			d->BaseColorTexture = CreateTexture(TexNames.find(aiTextureType_DIFFUSE)->second, core::FLinearColor(d->MatProperty.DiffuseColor));
			d->MetallicRoughnessTexture = CreateTexture(TexNames.find(aiTextureType_SHININESS)->second, core::FLinearColor(1.f, d->MatProperty.Shininess, d->MatProperty.Refracti, 1.f));
			d->EmissiveTexture = CreateTexture(TexNames.find(aiTextureType_EMISSIVE)->second, core::FLinearColor(1.f, 1.f, 1.f, 1.f));
			d->NormalTexture = CreateTexture(TexNames.find(aiTextureType_HEIGHT)->second, core::FLinearColor(1.f, 1.f, 1.f, 1.f));
			d->OcclusionTexture = CreateTexture(TexNames.find(aiTextureType_LIGHTMAP)->second, core::FLinearColor(1.f, 1.f, 1.f, 1.f));
			};

		ENQUEUE_UNIQUE_RENDER_COMMAND(CreateTexCommand);
	}

	std::string ObjMaterial::GetMaterialName() const
	{
		C_P(ObjMaterial);
		return d->MaterialName;
	}

	bool ObjMaterial::IsTransparent() const
	{
		C_P(ObjMaterial);
		return d->IsTransParent;
	}

	std::shared_ptr<RHITexture2D> ObjMaterial::GetBaseColorTexture() const
	{
		C_P(ObjMaterial);
		return d->BaseColorTexture;
	}

	std::shared_ptr<RHITexture2D> ObjMaterial::GetMetallicRoughnessTexture() const
	{
		C_P(ObjMaterial);
		return d->MetallicRoughnessTexture;
	}

	std::shared_ptr<RHITexture2D> ObjMaterial::GetNormalTexture() const
	{
		C_P(ObjMaterial);
		return d->NormalTexture;
	}

	std::shared_ptr<RHITexture2D> ObjMaterial::GetEmissiveTexture() const
	{
		C_P(ObjMaterial);
		return d->EmissiveTexture;
	}

	std::shared_ptr<RHITexture2D> ObjMaterial::GetOcclusionTexture() const
	{
		C_P(ObjMaterial);
		return d->OcclusionTexture;
	}

	const Engine::MaterialConfig& ObjMaterial::GetMaterialConfig() const
	{
		C_P(ObjMaterial);
		return d->Config;
	}

	void ObjMaterial::loadTextureFromMaterial(aiTextureType vTextureType, const aiMaterial* vMat, std::map< int32_t, aiString>& TexNames)
	{
		aiString TexName;
		int32_t TextureCount = vMat->GetTextureCount(vTextureType);
		if (TextureCount > 0)
		{
			vMat->GetTexture(vTextureType, 0, &TexName);
		}
		TexNames.insert({ vTextureType,TexName });
	}

}