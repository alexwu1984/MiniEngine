#include "Engine/JsonConfig.h"
#include "core/logger.h"

namespace Engine
{
	bool LoadJsonFile(const std::wstring& FileName, nlohmann::json& OutJson)
	{
		std::ifstream JsonFile(FileName);
		if (!JsonFile.is_open())
		{
			core::LOG(core::log_war, L"LoadJsonFile failed to open: %s", FileName.c_str());
			return false;
		}

		try
		{
			JsonFile >> OutJson;
		}
		catch (const std::exception& e)
		{
			core::LOG(core::log_err, L"LoadJsonFile parse failed: %s, error: %S", FileName.c_str(), e.what());
			return false;
		}

		return true;
	}
}
