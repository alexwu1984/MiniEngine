#include "GltfModel/GltfModelConfig.h"
#include "json.h"
#include "Scene/GltfMeshComponent.h"
#include "GltfModel/DynamicBoneInfo.h"
#include "core/strings.h"

namespace Engine
{
	struct GltfModelConfigPrivate
	{
		nlohmann::json Config;
		std::weak_ptr< GltfMeshComponent> Owner;
		std::vector< DynamicBoneInfo> DyBonelist;
		std::wstring ModelName;
		GltfFurConfig FurConfig;
	};

	GltfModelConfig::GltfModelConfig(std::weak_ptr< GltfMeshComponent> Owner)
		:d_ptr(new GltfModelConfigPrivate())
	{
		C_P(GltfModelConfig);
		d->Owner = Owner;
	}

	GltfModelConfig::~GltfModelConfig()
	{
		delete d_ptr;
	}

	bool GltfModelConfig::Load(const std::wstring& FileName)
	{
		C_P(GltfModelConfig);
		std::ifstream input_json_file(FileName);
		if (!input_json_file.is_open())
		{
			return false;
		}

		input_json_file >> d->Config;

		if (d->Config.is_null())
		{
			return false;
		}

		auto GltfJson = d->Config["Gltf"];
		if (GltfJson.is_null())
		{
			return false;
		}


		return Load(GltfJson);
	}

	bool GltfModelConfig::Load(const nlohmann::json& GltfJson)
	{
		C_P(GltfModelConfig);
		d->ModelName = core::u8_ucs2(GltfJson["Model"]);

		try
		{
			auto DyBoneListJson = d->Config["DyBone"];

			for (auto Item : DyBoneListJson)
			{
				DynamicBoneInfo BoneInfo;
				BoneInfo.BoneName = Item["Name"];
				BoneInfo.Damping = Item["Damping"].get<float>();
				BoneInfo.Elasticity = Item["Elasticity"].get<float>();
				BoneInfo.Stiffness = Item["Stiffness"].get<float>();
				BoneInfo.Inert = Item["Inert"].get<float>();
				d->DyBonelist.push_back(BoneInfo);
			}

			auto FurJson = d->Config["FurMaterial"];
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
		catch (const std::exception&)
		{
		}
		return !d->ModelName.empty();
	}

	std::wstring GltfModelConfig::GetModel() const
	{
		C_P(const GltfModelConfig);
		return d->ModelName;
	}

	const std::vector< DynamicBoneInfo>& GltfModelConfig::GetDyNamicBoneInfoList() const
	{
		C_P(const GltfModelConfig);
		return d->DyBonelist;
	}

	const Engine::GltfFurConfig& GltfModelConfig::GetFurConfig() const
	{
		C_P(const GltfModelConfig);
		return d->FurConfig;
	}

}