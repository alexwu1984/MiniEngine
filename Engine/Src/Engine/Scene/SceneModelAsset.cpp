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
		bool DisableAnimation = false;
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
		d->ModelRelativePath.clear();
		try
		{
			const auto mit = ModelJson.find("Model");
			if (mit != ModelJson.end() && mit->is_string())
				d->ModelRelativePath = core::u8_ucs2(mit->get<std::string>());
		}
		catch (const std::exception&)
		{
		}
		if (d->ModelRelativePath.empty())
			return false;

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
			// Allow "Material": {} — only read keys that exist and are the right JSON type (nlohmann [] on missing key yields null and can throw on .get<float>()).
			if (d->Config.find("Material") != d->Config.end() && d->Config["Material"].is_object())
			{
				const auto& mj = d->Config["Material"];
				auto tryFloat = [&mj](const char* key, float& out) {
					const auto it = mj.find(key);
					if (it == mj.end() || it->is_null() || !it->is_number())
						return;
					try
					{
						out = it->get<float>();
					}
					catch (const std::exception&)
					{
					}
				};
				tryFloat("Metallic", d->MaterialConfig.Metallic);
				tryFloat("Roughness", d->MaterialConfig.Roughness);
				const auto bcIt = mj.find("BaseColor");
				if (bcIt != mj.end() && bcIt->is_string())
				{
					const std::string& BaseColor = bcIt->get<std::string>();
					sscanf_s(BaseColor.c_str(), "%f,%f,%f,%f", &d->MaterialConfig.BaseColor.x, &d->MaterialConfig.BaseColor.y, &d->MaterialConfig.BaseColor.z,
							 &d->MaterialConfig.BaseColor.w);
				}
			}
			if(d->Config.find("UseMaterial") != d->Config.end())
				d->MaterialConfig.UseConfig = d->Config["UseMaterial"];
			if (d->Config.find("DisableAnimation") != d->Config.end() && !d->Config["DisableAnimation"].is_null())
				d->DisableAnimation = d->Config["DisableAnimation"].get<bool>();
		}
		catch (const std::exception&)
		{
		}
		return true;
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

	bool SceneModelAsset::GetDisableAnimation() const
	{
		C_P(const SceneModelAsset);
		return d->DisableAnimation;
	}
}