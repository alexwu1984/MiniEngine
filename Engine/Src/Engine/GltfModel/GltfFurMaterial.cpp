#include "GltfModel/GltfFurMaterial.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "RHI/RHITexture2D.h"
#include "Thread/RenderThread.h"
#include "core/color.h"
#include "GltfModel/GltfModel.h"
#include "GltfModel/GltfModelConfig.h"
#include "core/system.h"

namespace Engine
{
	using namespace RenderCore;

	struct GltfFurMaterialPrivate
	{
		std::shared_ptr<RenderCore::RHITexture2D> NoiseTex;
		GltfFurConfig FurConfig;
	};

	GltfFurMaterial::GltfFurMaterial(GltfModel* Owner, tinygltf::Model* Model)
		:GltfMaterial(Owner,Model)
		,d_ptr(new GltfFurMaterialPrivate())
	{

	}

	GltfFurMaterial::~GltfFurMaterial()
	{
		delete d_ptr;
	}

	void GltfFurMaterial::InitMaterial(uint32_t MaterialIndex)
	{
		GltfMaterial::InitMaterial(MaterialIndex);
		SetTransparent(false);

		C_P(GltfFurMaterial);

		const auto& ModelConfig = GetOwner()->GetModelConfig();
		d->FurConfig = ModelConfig->GetFurConfig();

		auto CreateTexCommand = [this, ModelConfig](DynamicRHI* DyRHI) {
			C_P(GltfFurMaterial);
			core::filesystem::path Path = core::process_directory();
			std::wstring ModelFile = Path.wstring() + L"/GLTFModel/" + core::u8_ucs2(ModelConfig->GetFurConfig().NoiseTex);
			
			d->NoiseTex = DyRHI->RHICreateTexture2D(ModelFile);
		};

		ENQUEUE_UNIQUE_RENDER_COMMAND(CreateTexCommand);
	}

	GltfMaterial::MaterialType GltfFurMaterial::GetMaterialType() const
	{
		return MaterialType::FUR;
	}

	const GltfFurConfig& GltfFurMaterial::GetFurConfig() const
	{
		C_P(const GltfFurMaterial);
		return d_ptr->FurConfig;
	}

	std::shared_ptr<RenderCore::RHITexture2D> GltfFurMaterial::GetNoiseTex() const
	{
		C_P(const GltfFurMaterial);
		return d_ptr->NoiseTex;
	}

}
