#include "Scene/SceneModelAsset.h"
#include "json.h"
#include "GltfModel/DynamicBoneInfo.h"
#include "core/strings.h"

namespace Engine
{
	struct SceneModelAssetPrivate
	{
		nlohmann::json Config;
		std::vector< FDynamicBoneInfo> DyBonelist;
		std::wstring ModelRelativePath;
		FurConfig FurConfig;
		MaterialConfig MaterialConfig;
	};

	SceneModelAsset::SceneModelAsset()
		:d_ptr(new SceneModelAssetPrivate())
	{
		C_P(SceneModelAsset);
	}

	SceneModelAsset::~SceneModelAsset()
	{
		delete d_ptr;
	}

	bool SceneModelAsset::Load(const nlohmann::json& ModelJson)
	{
		C_P(SceneModelAsset);
		d->Config = ModelJson;
		d->ModelRelativePath = core::u8_ucs2(ModelJson["Model"]);

		try
		{
			if (d->Config.find("DyBone") != d->Config.end())
			{
				const auto& DyBoneListJson = d->Config["DyBone"];
				for (auto Item : DyBoneListJson)
				{
					FDynamicBoneInfo BoneInfo;
					BoneInfo.BoneName = Item["Name"];
					BoneInfo.Damping = Item["Damping"].get<float>();
					BoneInfo.Elasticity = Item["Elasticity"].get<float>();
					BoneInfo.Stiffness = Item["Stiffness"].get<float>();
					BoneInfo.Inert = Item["Inert"].get<float>();
					BoneInfo.UpdateScale = Item["UpdateScale"].get<float>();
					d->DyBonelist.push_back(BoneInfo);
				}
			}
			if (d->Config.find("FurMaterial") != d->Config.end())
			{
				const auto& FurJson = d->Config["FurMaterial"];
				d->FurConfig.Name = FurJson["Name"];
				d->FurConfig.NoiseTex = FurJson["NoiseTex"];
				d->FurConfig.FurLength = FurJson["FurLength"];
				d->FurConfig.FurAmbientStrength = FurJson["FurAmbientStrength"];
				d->FurConfig.FurLevel = FurJson["FurLevel"];
				d->FurConfig.UVScale = FurJson["UVScale"];
				d->FurConfig.FurLightExposure = FurJson["FurLightExposure"];
				std::string Gravity = FurJson["Gravity"];
				sscanf_s(Gravity.c_str(), "%f,%f,%f", &d->FurConfig.Gravity.x, &d->FurConfig.Gravity.y, &d->FurConfig.Gravity.z);
			}
			if (d->Config.find("Material") != d->Config.end())
			{
				const auto& MaterialJson = d->Config["Material"];
				d->MaterialConfig.Metallic = MaterialJson["Metallic"];
				if(MaterialJson.count("Roughness"))
					d->MaterialConfig.Roughness = MaterialJson["Roughness"];
				if (MaterialJson.count("BaseColor"))
				{
					std::string BaseColor = MaterialJson["BaseColor"];
					sscanf_s(BaseColor.c_str(), "%f,%f,%f,%f", &d->MaterialConfig.BaseColor.x, &d->MaterialConfig.BaseColor.y, &d->MaterialConfig.BaseColor.z, &d->MaterialConfig.BaseColor.w);
				}
			}
			if(d->Config.find("UseMaterial") != d->Config.end())
				d->MaterialConfig.UseConfig = d->Config["UseMaterial"];
		}
		catch (const std::exception&)
		{
		}
		return !d->ModelRelativePath.empty();
	}

	std::wstring SceneModelAsset::GetModelRelativePath() const
	{
		C_P(const SceneModelAsset);
		return d->ModelRelativePath;
	}

	const std::vector< FDynamicBoneInfo>& SceneModelAsset::GetDyNamicBoneInfoList() const
	{
		C_P(const SceneModelAsset);
		return d->DyBonelist;
	}

	const Engine::FurConfig& SceneModelAsset::GetFurConfig() const
	{
		C_P(const SceneModelAsset);
		return d->FurConfig;
	}

	const Engine::MaterialConfig& SceneModelAsset::GetMaterialConfig() const
	{
		C_P(const SceneModelAsset);
		return d->MaterialConfig;
	}
}