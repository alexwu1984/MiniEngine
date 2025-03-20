#include "Material/GltfFurMaterial.h"
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
		FurConfig FurConfigParam;
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
		d->FurConfigParam = ModelConfig->GetFurConfig();

		core::filesystem::path Path = core::process_directory();
		std::wstring ModelFile = Path.wstring() + L"/GLTFModel/" + core::u8_ucs2(ModelConfig->GetFurConfig().NoiseTex);

		d->NoiseTex = GetDynamicRHI()->RHICreateTexture2D(ModelFile);
	}

	GltfMaterial::MaterialType GltfFurMaterial::GetMaterialType() const
	{
		return MaterialType::FUR;
	}

	const FurConfig& GltfFurMaterial::GetFurConfig() const
	{
		C_P(const GltfFurMaterial);
		return d_ptr->FurConfigParam;
	}

	std::shared_ptr<RenderCore::RHITexture2D> GltfFurMaterial::GetNoiseTex() const
	{
		C_P(const GltfFurMaterial);
		return d_ptr->NoiseTex;
	}

}
