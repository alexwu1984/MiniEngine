#include "Material/GltfMaterial.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "Thread/RenderThread.h"
#include "RHI/RHITexture2D.h"
#include "core/color.h"
#include "GltfModel/GltfModel.h"
#include "Scene/SceneModelAsset.h"

namespace Engine
{
	using namespace RenderCore;
	using namespace math;

	namespace
	{
		float ReadGltfExtNumber(const tinygltf::Value& V, float Default)
		{
			if (V.IsNumber())
				return static_cast<float>(V.GetNumberAsDouble());
			return Default;
		}

		int ReadGltfExtTextureIndex(const tinygltf::Value& TexInfo)
		{
			if (!TexInfo.IsObject() || !TexInfo.Has("index"))
				return -1;
			return TexInfo.Get("index").GetNumberAsInt();
		}
	}

	struct GltfMaterialPrivate
	{
		GltfModel* Owner = nullptr;
		tinygltf::Model* Model = nullptr;
		std::string MaterialName;
		bool DoubleSided = false;
		bool IsTransParent = false;
		bool UsesAlphaMask = false;
		float AlphaCutoff = 0.5f;
		/** BLEND + baseColorTexture: write depth in base pass (UE-style textured translucency). */
		bool WritesTranslucentDepth = false;
		bool UsesTransmission = false;
		float TransmissionFactor = 0.f;
		float AttenuationDistance = 1.f;
		Vector3 AttenuationColor = Vector3(1.f, 1.f, 1.f);
		float ThicknessFactor = 1.f;
		float MaterialIor = 1.5f;
		float MaterialDispersion = 0.f;

		std::shared_ptr<RHITexture2D> BaseColorTexture;
		std::shared_ptr<RHITexture2D> MetallicRoughnessTexture;
		std::shared_ptr<RHITexture2D> NormalTexture;
		std::shared_ptr<RHITexture2D> EmissiveTexture;
		std::shared_ptr<RHITexture2D> OcclusionTexture;
	};

	GltfMaterial::GltfMaterial(GltfModel* Owner,tinygltf::Model* Model)
		:d_ptr(new GltfMaterialPrivate())
	{
		C_P(GltfMaterial);
		d->Model = Model;
		d->Owner = Owner;
	}

	GltfMaterial::~GltfMaterial()
	{
		// Drain is handled once per bulk actor purge (World) or per SceneMeshComponent destroy — not here (N× stall).
		delete d_ptr;
	}

	void GltfMaterial::InitMaterial(uint32_t MaterialIndex)
	{
		C_P(GltfMaterial);

		auto CreateTexture = [this](int32_t Index, const core::FLinearColor& Color,bool useColor) {
			C_P(GltfMaterial);
			auto& gltfTexture = d->Model->textures;
			std::shared_ptr<RHITexture2D> TexRHI;
			if (Index > -1 && Index < gltfTexture.size() && !useColor)
			{
				int32_t Source = gltfTexture[Index].source;
				auto& ModelImage = d->Model->images[Source];
				uint8_t* pData = (uint8_t*)ModelImage.image.data();
				TexRHI = GEngine->GetRHI()->RHICreateTexture2D(EPixelFormat::PF_R8G8B8A8, RenderCore::TexCreate_ShaderResource, ModelImage.width, ModelImage.height, 1, pData, ModelImage.width * 4);
			}
			else
			{
				TexRHI = GEngine->GetRHI()->RHICreateTexture2D(Color);
			}

			return TexRHI;
			};

		if (d->Model->materials.empty())
		{
			d->WritesTranslucentDepth = false;
			auto CreateTexCommand = [this,CreateTexture](DynamicRHI* DyRHI) {
				C_P(GltfMaterial);
				d->BaseColorTexture = CreateTexture(-1, core::FLinearColor(1.f, 1.0f, 1.f, 1.f), true);
				d->MetallicRoughnessTexture = CreateTexture(-1, core::FLinearColor(1.f, 1.f, 1.f, 1.0), true);
				d->EmissiveTexture = CreateTexture(-1, core::FLinearColor(1.f, 1.f, 1.f, 1.0), true);
				d->NormalTexture = CreateTexture(-1, core::FLinearColor(1.f, 1.f, 1.f, 1.0), true);
				d->OcclusionTexture = CreateTexture(-1, core::FLinearColor(1.f, 1.f, 1.f, 1.0), true);
				};

			ENQUEUE_UNIQUE_RENDER_COMMAND(CreateTexCommand);
			FlushRenderingCommands(ERenderQueueFlushCategory::LoadOrResourceCreationSync);
		}
		else
		{
			auto& Material = d->Model->materials[MaterialIndex];

			d->MaterialName = Material.name;
			// glTF defaults omit doubleSided (tinygltf: doubleSidedSpecified=false); many BLEND shells expect both faces.
			// Explicit "doubleSided": false (e.g. busterDrone BLEND floor) must stay single-sided.
			d->DoubleSided = Material.doubleSided;
			if (!Material.doubleSidedSpecified && Material.alphaMode == "BLEND")
				d->DoubleSided = true;

			// glTF: only BLEND uses order-dependent transparency; MASK is a binary test (clip), not blending.
			d->IsTransParent = (Material.alphaMode == "BLEND");
			d->UsesAlphaMask = (Material.alphaMode == "MASK");
			d->AlphaCutoff = static_cast<float>(Material.alphaCutoff);
			const int bcIdx = Material.pbrMetallicRoughness.baseColorTexture.index;

			d->UsesTransmission = false;
			d->TransmissionFactor = 0.f;
			d->AttenuationDistance = 1.f;
			d->AttenuationColor = Vector3(1.f, 1.f, 1.f);
			d->ThicknessFactor = 1.f;
			d->MaterialIor = 1.5f;
			d->MaterialDispersion = 0.f;
			int thicknessTexIdx = -1;

			if (const auto iorIt = Material.extensions.find("KHR_materials_ior"); iorIt != Material.extensions.end())
			{
				const tinygltf::Value& iorExt = iorIt->second;
				if (iorExt.IsObject())
					d->MaterialIor = math::Max(ReadGltfExtNumber(iorExt.Get("ior"), 1.5f), 1.0f);
			}
			if (const auto dispIt = Material.extensions.find("KHR_materials_dispersion"); dispIt != Material.extensions.end())
			{
				const tinygltf::Value& dispExt = dispIt->second;
				if (dispExt.IsObject())
					d->MaterialDispersion = math::Max(ReadGltfExtNumber(dispExt.Get("dispersion"), 0.f), 0.f);
			}

			if (const auto transIt = Material.extensions.find("KHR_materials_transmission"); transIt != Material.extensions.end())
			{
				const tinygltf::Value& transExt = transIt->second;
				if (transExt.IsObject())
				{
					d->TransmissionFactor = ReadGltfExtNumber(transExt.Get("transmissionFactor"), 0.f);
					if (d->TransmissionFactor > 1e-4f)
					{
						d->UsesTransmission = true;
						d->IsTransParent = true;
					}
				}
			}
			if (const auto volIt = Material.extensions.find("KHR_materials_volume"); volIt != Material.extensions.end())
			{
				const tinygltf::Value& volExt = volIt->second;
				if (volExt.IsObject())
				{
					const tinygltf::Value& attColor = volExt.Get("attenuationColor");
					if (attColor.IsArray() && attColor.ArrayLen() >= 3u)
					{
						d->AttenuationColor.x = ReadGltfExtNumber(attColor.Get(0), 1.f);
						d->AttenuationColor.y = ReadGltfExtNumber(attColor.Get(1), 1.f);
						d->AttenuationColor.z = ReadGltfExtNumber(attColor.Get(2), 1.f);
					}
					d->AttenuationDistance = math::Max(ReadGltfExtNumber(volExt.Get("attenuationDistance"), 1.f), 1e-4f);
					d->ThicknessFactor = ReadGltfExtNumber(volExt.Get("thicknessFactor"), 1.f);
					thicknessTexIdx = ReadGltfExtTextureIndex(volExt.Get("thicknessTexture"));
				}
			}

			d->WritesTranslucentDepth = (Material.alphaMode == "BLEND") && (bcIdx >= 0);

			// glTF default baseColorFactor is opaque white; attenuationColor is volume-only (KHR_materials_volume).
			const core::FLinearColor baseFallback = GetMaterialConfig().UseConfig
				? core::FLinearColor(GetMaterialConfig().BaseColor)
				: core::FLinearColor(1.f, 1.f, 1.f, 1.f);
			const bool bUseJsonMaterialOverride = GetMaterialConfig().UseConfig;

			auto CreateTexCommand = [this, Material, CreateTexture, baseFallback, bUseJsonMaterialOverride, thicknessTexIdx](DynamicRHI* DyRHI) {
				C_P(GltfMaterial);
				int32_t Index = Material.pbrMetallicRoughness.baseColorTexture.index;
				d->BaseColorTexture = CreateTexture(Index, baseFallback, bUseJsonMaterialOverride);

				Index = Material.pbrMetallicRoughness.metallicRoughnessTexture.index;
				const float gltfRough = static_cast<float>(Material.pbrMetallicRoughness.roughnessFactor);
				const float gltfMetal = static_cast<float>(Material.pbrMetallicRoughness.metallicFactor);
				d->MetallicRoughnessTexture = CreateTexture(
					Index,
					core::FLinearColor(1.f, gltfRough, gltfMetal, 1.0),
					bUseJsonMaterialOverride);

				auto EmissiveColor = Material.emissiveFactor;
				Index = Material.emissiveTexture.index;
				d->EmissiveTexture = CreateTexture(Index, core::FLinearColor(float(EmissiveColor[0]), float(EmissiveColor[0]), float(EmissiveColor[1]), float(EmissiveColor[2])), bUseJsonMaterialOverride);

				Index = Material.normalTexture.index;
				d->NormalTexture = CreateTexture(Index, core::FLinearColor(0.5f, 0.5f, 1.f, 1.f), false);

				// glTF: missing occlusion → factor 1.0 (no darkening). KHR_materials_volume thickness is in G channel.
				Index = d->UsesTransmission ? thicknessTexIdx : Material.occlusionTexture.index;
				d->OcclusionTexture = CreateTexture(Index, core::FLinearColor(1.f, 1.f, 1.f, 1.f), false);
				};

			ENQUEUE_UNIQUE_RENDER_COMMAND(CreateTexCommand);
			FlushRenderingCommands(ERenderQueueFlushCategory::LoadOrResourceCreationSync);
		}

	}

	std::string GltfMaterial::GetMaterialName() const
	{
		C_P(const GltfMaterial);
		return d->MaterialName;
	}

	bool GltfMaterial::IsTransparent() const
	{
		C_P(const GltfMaterial);
		return d->IsTransParent;
	}

	bool GltfMaterial::WritesTranslucentDepthToSceneBuffer() const
	{
		C_P(const GltfMaterial);
		return d->WritesTranslucentDepth;
	}

	bool GltfMaterial::UsesMaterialAlphaMask() const
	{
		C_P(const GltfMaterial);
		return d->UsesAlphaMask;
	}

	float GltfMaterial::GetMaterialAlphaCutoff() const
	{
		C_P(const GltfMaterial);
		return d->AlphaCutoff;
	}

	bool GltfMaterial::IsDoubleSided() const
	{
		C_P(const GltfMaterial);
		return d->DoubleSided;
	}

	bool GltfMaterial::UsesTransmissionShading() const
	{
		C_P(const GltfMaterial);
		return d->UsesTransmission;
	}

	float GltfMaterial::GetTransmissionFactor() const
	{
		C_P(const GltfMaterial);
		return d->TransmissionFactor;
	}

	float GltfMaterial::GetAttenuationDistance() const
	{
		C_P(const GltfMaterial);
		return d->AttenuationDistance;
	}

	math::Vector3 GltfMaterial::GetAttenuationColor() const
	{
		C_P(const GltfMaterial);
		return d->AttenuationColor;
	}

	float GltfMaterial::GetThicknessFactor() const
	{
		C_P(const GltfMaterial);
		return d->ThicknessFactor;
	}

	float GltfMaterial::GetMaterialIor() const
	{
		C_P(const GltfMaterial);
		return d->MaterialIor;
	}

	float GltfMaterial::GetMaterialDispersion() const
	{
		C_P(const GltfMaterial);
		return d->MaterialDispersion;
	}

	bool GltfMaterial::WantsRHIBindless() const
	{
		return true;
	}

	void GltfMaterial::SetTransparent(bool Transparent)
	{
		C_P(GltfMaterial);
		d->IsTransParent = Transparent;
	}

	GltfMaterial::MaterialType GltfMaterial::GetMaterialType() const
	{
		return MaterialType::PBR;
	}

	std::shared_ptr<RHITexture2D> GltfMaterial::GetBaseColorTexture() const
	{
		C_P(GltfMaterial);
		return d->BaseColorTexture;
	}

	std::shared_ptr<RHITexture2D> GltfMaterial::GetMetallicRoughnessTexture() const
	{
		C_P(GltfMaterial);
		return d->MetallicRoughnessTexture;
	}

	std::shared_ptr<RHITexture2D> GltfMaterial::GetNormalTexture() const
	{
		C_P(GltfMaterial);
		return d->NormalTexture;
	}

	std::shared_ptr<RHITexture2D> GltfMaterial::GetEmissiveTexture() const
	{
		C_P(GltfMaterial);
		return d->EmissiveTexture;
	}

	std::shared_ptr<RenderCore::RHITexture2D> GltfMaterial::GetOcclusionTexture() const
	{
		C_P(GltfMaterial);
		return d->OcclusionTexture;
	}


	const Engine::MaterialConfig& GltfMaterial::GetMaterialConfig() const
	{
		C_P(GltfMaterial);
		if (auto Asset = d->Owner ? d->Owner->GetAsset() : nullptr)
			return Asset->GetMaterialConfig();
		static MaterialConfig Fallback{};
		return Fallback;
	}

	tinygltf::Model* GltfMaterial::GetModel()
	{
		C_P(GltfMaterial);
		return d->Model;
	}

	GltfModel* GltfMaterial::GetOwner()
	{
		C_P(GltfMaterial);
		return d->Owner;
	}

}