#include "Material/FurMaterial.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "RHI/RHITexture2D.h"
#include "Thread/RenderThread.h"
#include "core/color.h"
#include "GltfModel/GltfModel.h"
#include "Scene/SceneModelAsset.h"
#include "core/system.h"

namespace Engine
{
	using namespace RenderCore;

	struct FurMaterialPrivate
	{
		std::shared_ptr<RenderCore::RHITexture2D> NoiseTex;
		FurConfig FurConfigParam;
	};

	FurMaterial::FurMaterial(GltfModel* Owner, tinygltf::Model* Model)
		:GltfMaterial(Owner,Model)
		,d_ptr(new FurMaterialPrivate())
	{

	}

	FurMaterial::~FurMaterial()
	{
		delete d_ptr;
	}

	void FurMaterial::InitMaterial(uint32_t MaterialIndex)
	{
		GltfMaterial::InitMaterial(MaterialIndex);
		SetTransparent(false);

		C_P(FurMaterial);

		const auto Asset = GetOwner() ? GetOwner()->GetAsset() : nullptr;
		if (Asset)
			d->FurConfigParam = Asset->GetFurConfig();

		core::filesystem::path Path = core::process_directory();
		std::wstring ModelFile = Path.wstring() + L"/GLTFModel/" + core::u8_ucs2(d->FurConfigParam.NoiseTex);

		d->NoiseTex = GetDynamicRHI()->RHICreateTexture2D(ModelFile);
	}

	GltfMaterial::MaterialType FurMaterial::GetMaterialType() const
	{
		return MaterialType::FUR;
	}

	const FurConfig& FurMaterial::GetFurConfig() const
	{
		C_P(const FurMaterial);
		return d_ptr->FurConfigParam;
	}

	std::shared_ptr<RenderCore::RHITexture2D> FurMaterial::GetNoiseTex() const
	{
		C_P(const FurMaterial);
		return d_ptr->NoiseTex;
	}

}
