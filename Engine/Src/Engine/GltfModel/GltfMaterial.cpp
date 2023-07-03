#include "GltfModel/GltfMaterial.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "RHI/RHITexture2D.h"
#include "Thread/RenderThread.h"
#include "core/color.h"
#include "GltfModel/GltfModel.h"
#include "GltfModel/GltfModelConfig.h"

namespace Engine
{
	using namespace RenderCore;
	using namespace math;

	struct GltfMaterialPrivate
	{
		GltfModel* Owner = nullptr;
		tinygltf::Model* Model = nullptr;
		std::string MaterialName;
		bool DoubleSided = false;                
		bool IsTransParent = false;

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
		if (GRenderThread)
		{
			GRenderThread->WaitForFinish();
		}
		delete d_ptr;
	}

	void GltfMaterial::InitMaterial(uint32_t MaterialIndex)
	{
		C_P(GltfMaterial);
		auto& Material = d->Model->materials[MaterialIndex];

		d->MaterialName = Material.name;
		d->DoubleSided = Material.doubleSided;

		d->IsTransParent = (Material.alphaMode != "OPAQUE");

		auto CreateTexture = [this](int32_t Index,const core::FLinearColor& Color) {
			C_P(GltfMaterial);
			auto& gltfTexture = d->Model->textures;
			std::shared_ptr<RHITexture2D> TexRHI;
			if (Index > -1 && Index < gltfTexture.size())
			{
				int32_t Source = gltfTexture[Index].source;
				auto& ModelImage = d->Model->images[Source];
				uint8_t* pData = (uint8_t*)ModelImage.image.data();
				TexRHI = GEngine->GetRHI()->RHICreateTexture2D(EPixelFormat::PF_R8G8B8A8, RenderCore::TexCreate_ShaderResource, ModelImage.width, ModelImage.height, pData, ModelImage.width * 4);
			}
			else
			{
				TexRHI = GEngine->GetRHI()->RHICreateTexture2D(Color);
			}

			return TexRHI;
		};

		auto CreateTexCommand = [this, Material, CreateTexture](DynamicRHI *DyRHI) {
			C_P(GltfMaterial);
			int32_t Index = Material.pbrMetallicRoughness.baseColorTexture.index;
			d->BaseColorTexture = CreateTexture(Index, core::FLinearColor(1.f, 1.0f, 1.f, 1.f));

			Index = Material.pbrMetallicRoughness.metallicRoughnessTexture.index;
			d->MetallicRoughnessTexture = CreateTexture(Index, core::FLinearColor(1.f, float(Material.pbrMetallicRoughness.roughnessFactor), float(Material.pbrMetallicRoughness.metallicFactor), 1.0));

			auto EmissiveColor = Material.emissiveFactor;
			Index = Material.emissiveTexture.index;
			d->EmissiveTexture = CreateTexture(Index, core::FLinearColor(float(EmissiveColor[0]), float(EmissiveColor[0]), float(EmissiveColor[1]), float(EmissiveColor[2])));

			Index = Material.normalTexture.index;
			d->NormalTexture = CreateTexture(Index, core::FLinearColor(0.5f, 0.5f, 1.f, 1.f));

			Index = Material.occlusionTexture.index;
			d->OcclusionTexture = CreateTexture(Index, core::FLinearColor(0.5f, 0.5f, 1.f, 1.f));
		};

		ENQUEUE_UNIQUE_RENDER_COMMAND(CreateTexCommand);
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

	void GltfMaterial::SetTransparent(bool Transparent)
	{
		C_P(GltfMaterial);
		d->IsTransParent = Transparent;
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

}