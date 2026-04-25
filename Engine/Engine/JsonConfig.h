#pragma once
#include "core/inc.h"
#include "tinygltf/json.h"

namespace Engine
{
	bool LoadJsonFile(const std::wstring& FileName, nlohmann::json& OutJson);
}
