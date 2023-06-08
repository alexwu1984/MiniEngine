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
	};

	GltfModelConfig::GltfModelConfig(std::weak_ptr< GltfMeshComponent> Owner)
		:Impl(new GltfModelConfigPrivate())
	{
		Impl->Owner = Owner;
	}

	GltfModelConfig::~GltfModelConfig()
	{
		delete Impl;
	}

	bool GltfModelConfig::Load(const std::wstring& FileName)
	{
		std::ifstream input_json_file(FileName);
		if (!input_json_file.is_open())
		{
			return false;
		}

		input_json_file >> Impl->Config;

		if (Impl->Config.is_null())
		{
			return false;
		}

		auto GltfJson = Impl->Config["Gltf"];
		if (GltfJson.is_null())
		{
			return false;
		}
		Impl->ModelName = core::u8_ucs2(GltfJson["Model"]);

		auto DyBoneListJson = Impl->Config["DyBone"];

		for (auto Item: DyBoneListJson)
		{
			DynamicBoneInfo BoneInfo;
			BoneInfo.BoneName = Item["Name"];
			BoneInfo.Damping = Item["Damping"].get<float>();
			BoneInfo.Elasticity = Item["Elasticity"].get<float>();
			BoneInfo.Stiffness = Item["Stiffness"].get<float>();
			BoneInfo.Inert = Item["Inert"].get<float>();
			Impl->DyBonelist.push_back(BoneInfo);
		}

		return true;
	}

	std::wstring GltfModelConfig::GetModel() const
	{
		return Impl->ModelName;
	}

}