#include "GltfModel/GltfMaterial.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "RHI/RHITexture2D.h"
#include "Thread/RenderThread.h"
#include "core/color.h"

namespace Engine
{
	using namespace RenderCore;
	using namespace math;

	struct GltfMaterialP
	{
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

	GltfMaterial::GltfMaterial(tinygltf::Model* Model)
		:Data(std::make_shared<GltfMaterialP>())
	{
		Data->Model = Model;
	}

	GltfMaterial::~GltfMaterial()
	{
		if (GRenderThread)
		{
			GRenderThread->WaitForFinish();
		}
	}

	void GltfMaterial::InitMaterial(uint32_t MaterialIndex)
	{
		auto& Material = Data->Model->materials[MaterialIndex];

		Data->MaterialName = Material.name;
		Data->DoubleSided = Material.doubleSided;
		Data->IsTransParent = (Material.alphaMode != "OPAQUE");

		
		auto CreateTexture = [Data = Data](int32_t Index,const core::FLinearColor& Color) {
			auto& gltfTexture = Data->Model->textures;
			std::shared_ptr<RHITexture2D> TexRHI;
			if (Index > -1 && Index < gltfTexture.size())
			{
				int32_t Source = gltfTexture[Index].source;
				auto& ModelImage = Data->Model->images[Source];
				uint8_t* pData = (uint8_t*)ModelImage.image.data();
				TexRHI = GEngine->GetRHI()->RHICreateTexture2D(EPixelFormat::PF_B8G8R8A8, RenderCore::TexCreate_ShaderResource, ModelImage.width, ModelImage.height, pData, ModelImage.width * 4);
			}
			else
			{
				TexRHI = GEngine->GetRHI()->RHICreateTexture2D(Color);
			}

			return TexRHI;
		};

		auto CreateTexCommand = [Data = Data, Material, CreateTexture](DynamicRHI *DyRHI) {

			int32_t Index = Material.pbrMetallicRoughness.baseColorTexture.index;
			Data->BaseColorTexture = CreateTexture(Index, core::FLinearColor(1.f, 1.0f, 1.f, 1.f));

			Index = Material.pbrMetallicRoughness.metallicRoughnessTexture.index;
			Data->MetallicRoughnessTexture = CreateTexture(Index, core::FLinearColor(1.f, float(Material.pbrMetallicRoughness.roughnessFactor), float(Material.pbrMetallicRoughness.metallicFactor), 1.0));

			auto EmissiveColor = Material.emissiveFactor;
			Index = Material.emissiveTexture.index;
			Data->EmissiveTexture = CreateTexture(Index, core::FLinearColor(float(EmissiveColor[0]), float(EmissiveColor[0]), float(EmissiveColor[1]), float(EmissiveColor[2])));

			Index = Material.normalTexture.index;
			Data->NormalTexture = CreateTexture(Index, core::FLinearColor(0.5f, 0.5f, 1.f, 1.f));

			Index = Material.occlusionTexture.index;
			Data->OcclusionTexture = CreateTexture(Index, core::FLinearColor(0.5f, 0.5f, 1.f, 1.f));
		};

		ENQUEUE_UNIQUE_RENDER_COMMAND(CreateTexCommand);
	}

	std::string GltfMaterial::GetMaterialName() const
	{
		return Data->MaterialName;
	}

	bool GltfMaterial::IsTransparent() const
	{
		return Data->IsTransParent;
	}

	std::shared_ptr<RHITexture2D> GltfMaterial::GetBaseColorTexture() const
	{
		return Data->BaseColorTexture;
	}

	std::shared_ptr<RHITexture2D> GltfMaterial::GetMetallicRoughnessTexture() const
	{
		return Data->MetallicRoughnessTexture;
	}

	std::shared_ptr<RHITexture2D> GltfMaterial::GetNormalTexture() const
	{
		return Data->NormalTexture;
	}

	std::shared_ptr<RHITexture2D> GltfMaterial::GetEmissiveTexture() const
	{
		return Data->EmissiveTexture;
	}

	std::shared_ptr<RenderCore::RHITexture2D> GltfMaterial::GetOcclusionTexture() const
	{
		return Data->OcclusionTexture;
	}

}