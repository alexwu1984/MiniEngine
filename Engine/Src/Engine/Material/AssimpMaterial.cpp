#include "Material/AssimpMaterial.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "RHI/RHITexture2D.h"
#include "Thread/RenderThread.h"
#include <filesystem>
#include <DirectXTex.h>
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

	struct AssimpMaterialPrivate
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

	AssimpMaterial::AssimpMaterial(const aiScene* pScene, aiMesh* pMesh, const std::string& Directory)
		:d_ptr(new AssimpMaterialPrivate())
	{
		C_P(AssimpMaterial);
		d->Scene = pScene;
		d->vAiMesh = pMesh;
		d->Directory = Directory;
	}

	AssimpMaterial::~AssimpMaterial()
	{
		delete d_ptr;
	}

	void AssimpMaterial::Init()
	{
		C_P(AssimpMaterial);
		if (d->vAiMesh->mMaterialIndex < 0)
			return;
		
		aiMaterial* pAiMat = d->Scene->mMaterials[d->vAiMesh->mMaterialIndex];
		// Some Assimp versions don't expose PBR-specific aiTextureType_* enums for OBJ extensions.
		// Collect all available texture paths and classify them by filename patterns.
		std::vector<std::string> allTex;
		auto collect = [&allTex, pAiMat](aiTextureType t) {
			const unsigned count = pAiMat->GetTextureCount(t);
			for (unsigned i = 0; i < count; ++i)
			{
				aiString s;
				if (pAiMat->GetTexture(t, i, &s) == aiReturn_SUCCESS && s.length > 0)
					allTex.emplace_back(s.C_Str());
			}
		};
		collect(aiTextureType_DIFFUSE);
		collect(aiTextureType_SHININESS);
		collect(aiTextureType_HEIGHT);
		collect(aiTextureType_NORMALS);
		collect(aiTextureType_EMISSIVE);
		collect(aiTextureType_LIGHTMAP);
		collect(aiTextureType_AMBIENT);
		collect(aiTextureType_SPECULAR);
		collect(aiTextureType_UNKNOWN);

		auto pickByName = [&allTex](const char* needleA, const char* needleB = nullptr) -> std::string {
			for (const auto& p : allTex)
			{
				std::string low = p;
				std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return (char)std::tolower(c); });
				if (low.find(needleA) != std::string::npos)
					return p;
				if (needleB && low.find(needleB) != std::string::npos)
					return p;
			}
			return {};
		};

		const std::string baseColorPath = pickByName("albedo", "diffuse");
		const std::string normalPath = pickByName("normal", "bump");
		const std::string ormPath = pickByName("orm", "occlusionroughnessmetallic");
		const std::string armPath = pickByName("arm"); // ambient-roughness-metallic
		std::string roughPath = pickByName("rough", "roughness");
		std::string metalPath = pickByName("metal", "metallic");
		std::string aoPath = pickByName("ao", "occlusion");
		const std::string emissPath = pickByName("emiss");

		// Combined textures: ORM/ARM often used as a single packed map (R=AO, G=Roughness, B=Metallic).
		const std::string combinedOrmLike = !ormPath.empty() ? ormPath : armPath;

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
			C_P(AssimpMaterial);
			std::shared_ptr<RHITexture2D> TexRHI;
			if (Str.length > 0)
			{
				// Use filesystem join to avoid slash issues (Directory may contain '\\' on Windows).
				const std::filesystem::path full = std::filesystem::u8path(d->Directory) / std::filesystem::u8path(Str.C_Str());
				TexRHI = GEngine->GetRHI()->RHICreateTexture2D(full.wstring());
				if (!TexRHI)
				{
					// Missing/failed texture should not black out the material; fall back to a constant.
					TexRHI = GEngine->GetRHI()->RHICreateTexture2D(Color);
				}
			}
			else
			{
				TexRHI = GEngine->GetRHI()->RHICreateTexture2D(Color);
			}

			return TexRHI;
		};

		auto CreateTexCommand = [this, CreateTexture, baseColorPath, normalPath, roughPath, metalPath, aoPath, emissPath, combinedOrmLike](DynamicRHI* DyRHI) {
			C_P(AssimpMaterial);
			auto toAi = [](const std::string& s) {
				aiString a;
				if (!s.empty())
					a = aiString(s);
				return a;
			};

			d->BaseColorTexture = CreateTexture(toAi(baseColorPath), core::FLinearColor(d->MatProperty.DiffuseColor));
			d->NormalTexture = CreateTexture(toAi(normalPath), core::FLinearColor(0.5f, 0.5f, 1.f, 1.f));
			d->EmissiveTexture = CreateTexture(toAi(emissPath), core::FLinearColor(0.f, 0.f, 0.f, 1.f));
			d->OcclusionTexture = CreateTexture(toAi(aoPath), core::FLinearColor(1.f, 1.f, 1.f, 1.f));

			// Prefer combined ORM/ARM packed maps when present.
			if (!combinedOrmLike.empty())
			{
				std::shared_ptr<RHITexture2D> packed = CreateTexture(toAi(combinedOrmLike), core::FLinearColor(1.f, 1.f, 0.f, 1.f));
				if (packed)
				{
					d->MetallicRoughnessTexture = packed;
					// If AO wasn't explicitly specified, reuse packed R channel as AO.
					if (aoPath.empty())
						d->OcclusionTexture = packed;
				}
			}

			// Metallic-Roughness
			// Our deferred path expects a combined texture. If both Metalness + Roughness exist, pack them (G=rough, B=metal).
			if (!d->MetallicRoughnessTexture && (!metalPath.empty() || !roughPath.empty()))
			{
				const std::filesystem::path metalFull = !metalPath.empty() ? (std::filesystem::u8path(d->Directory) / std::filesystem::u8path(metalPath)) : std::filesystem::path();
				const std::filesystem::path roughFull = !roughPath.empty() ? (std::filesystem::u8path(d->Directory) / std::filesystem::u8path(roughPath)) : std::filesystem::path();

				auto loadWicAsRgbaUnorm = [](const std::filesystem::path& fullPath, DirectX::ScratchImage& outRgba) -> bool {
					DirectX::ScratchImage raw;
					if (fullPath.empty())
						return false;
					if (FAILED(DirectX::LoadFromWICFile(fullPath.wstring().c_str(), DirectX::WIC_FLAGS_NONE, nullptr, raw)))
						return false;
					const DirectX::Image* src = raw.GetImage(0, 0, 0);
					if (!src || !src->pixels)
						return false;
					const HRESULT hrConvert = DirectX::Convert(*src, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, 0.f, outRgba);
					return SUCCEEDED(hrConvert) && outRgba.GetImage(0, 0, 0) && outRgba.GetImage(0, 0, 0)->pixels;
				};

				auto resizeTo = [](DirectX::ScratchImage& rgba, uint32_t tw, uint32_t th) -> bool {
					const DirectX::Image* img = rgba.GetImage(0, 0, 0);
					if (!img || !img->pixels || tw == 0 || th == 0)
						return false;
					if (img->width == tw && img->height == th)
						return true;
					DirectX::ScratchImage resized;
					if (FAILED(DirectX::Resize(*img, tw, th, DirectX::TEX_FILTER_DEFAULT, resized)))
						return false;
					rgba = std::move(resized);
					return rgba.GetImage(0, 0, 0) != nullptr;
				};

				DirectX::ScratchImage metalRgba, roughRgba;
				bool hasMetal = loadWicAsRgbaUnorm(metalFull, metalRgba);
				bool hasRough = loadWicAsRgbaUnorm(roughFull, roughRgba);

				uint32_t w = 0, h = 0;
				if (hasRough && roughRgba.GetImage(0, 0, 0))
				{
					w = (uint32_t)roughRgba.GetImage(0, 0, 0)->width;
					h = (uint32_t)roughRgba.GetImage(0, 0, 0)->height;
				}
				else if (hasMetal && metalRgba.GetImage(0, 0, 0))
				{
					w = (uint32_t)metalRgba.GetImage(0, 0, 0)->width;
					h = (uint32_t)metalRgba.GetImage(0, 0, 0)->height;
				}

				if (w > 0 && h > 0)
				{
					if (hasMetal && !resizeTo(metalRgba, w, h))
						hasMetal = false;
					if (hasRough && !resizeTo(roughRgba, w, h))
						hasRough = false;
				}

				if ((hasMetal || hasRough) && w > 0 && h > 0)
				{
					const DirectX::Image* m0 = hasMetal ? metalRgba.GetImage(0, 0, 0) : nullptr;
					const DirectX::Image* r0 = hasRough ? roughRgba.GetImage(0, 0, 0) : nullptr;

					std::vector<uint8_t> packed;
					packed.resize((size_t)w * (size_t)h * 4u);
					for (uint32_t y = 0; y < h; ++y)
					{
						for (uint32_t x = 0; x < w; ++x)
						{
							const size_t idx = ((size_t)y * w + x) * 4u;
							const uint8_t metalV = m0 ? m0->pixels[idx + 0] : 0; // grayscale in R
							const uint8_t roughV = r0 ? r0->pixels[idx + 0] : 255;
							packed[idx + 0] = 255;
							packed[idx + 1] = roughV; // G
							packed[idx + 2] = metalV; // B
							packed[idx + 3] = 255;
						}
					}

					d->MetallicRoughnessTexture = GEngine->GetRHI()->RHICreateTexture2D(RenderCore::PF_R8G8B8A8, RenderCore::TexCreate_ShaderResource, (int32_t)w, (int32_t)h, 1, packed.data(), (int)(w * 4u));
				}
			}
			if (!d->MetallicRoughnessTexture)
			{
				// Fallback constant: (R=1, G=roughness, B=metallic).
				// For legacy OBJ/MTL (no PBR maps), derive a plausible constant from Ks/Ns:
				// - Higher shininess => lower roughness
				// - Strong specular color => more "metal-like" response
				const float ks = (std::max)(d->MatProperty.SpecularColor.x, (std::max)(d->MatProperty.SpecularColor.y, d->MatProperty.SpecularColor.z));
				const float ns = (std::max)(d->MatProperty.Shininess, 0.0f);
				// Map Ns~[0, 1000] into perceptual roughness~[1, 0.05]
				const float ns01 = (std::min)(ns / 1000.0f, 1.0f);
				const float rough = (std::max)(0.05f, 1.0f - ns01);
				const float metal = (ks > 0.5f) ? 1.0f : 0.0f;
				d->MetallicRoughnessTexture = GEngine->GetRHI()->RHICreateTexture2D(core::FLinearColor(1.f, rough, metal, 1.f));
			}
			};

		ENQUEUE_UNIQUE_RENDER_COMMAND(CreateTexCommand);
	}

	std::string AssimpMaterial::GetMaterialName() const
	{
		C_P(AssimpMaterial);
		return d->MaterialName;
	}

	bool AssimpMaterial::IsTransparent() const
	{
		C_P(AssimpMaterial);
		return d->IsTransParent;
	}

	std::shared_ptr<RHITexture2D> AssimpMaterial::GetBaseColorTexture() const
	{
		C_P(AssimpMaterial);
		return d->BaseColorTexture;
	}

	std::shared_ptr<RHITexture2D> AssimpMaterial::GetMetallicRoughnessTexture() const
	{
		C_P(AssimpMaterial);
		return d->MetallicRoughnessTexture;
	}

	std::shared_ptr<RHITexture2D> AssimpMaterial::GetNormalTexture() const
	{
		C_P(AssimpMaterial);
		return d->NormalTexture;
	}

	std::shared_ptr<RHITexture2D> AssimpMaterial::GetEmissiveTexture() const
	{
		C_P(AssimpMaterial);
		return d->EmissiveTexture;
	}

	std::shared_ptr<RHITexture2D> AssimpMaterial::GetOcclusionTexture() const
	{
		C_P(AssimpMaterial);
		return d->OcclusionTexture;
	}

	const Engine::MaterialConfig& AssimpMaterial::GetMaterialConfig() const
	{
		C_P(AssimpMaterial);
		return d->Config;
	}

	void AssimpMaterial::loadTextureFromMaterial(aiTextureType vTextureType, const aiMaterial* vMat, std::map< int32_t, aiString>& TexNames)
	{
		aiString TexName;
		int32_t TextureCount = vMat->GetTextureCount(vTextureType);
		if (TextureCount > 0)
		{
			vMat->GetTexture(vTextureType, 0, &TexName);
		}
		TexNames.insert({ (int32_t)vTextureType,TexName });
	}

}