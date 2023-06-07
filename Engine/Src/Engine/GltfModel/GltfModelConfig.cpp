#include "GltfModel/GltfModelConfig.h"
#include "json.h"
#include "Scene/GltfMeshComponent.h"

namespace Engine
{
	struct GltfModelConfigPrivate
	{
		nlohmann::json Config;
		std::weak_ptr< GltfMeshComponent> Owner;
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

		return true;
	}

}