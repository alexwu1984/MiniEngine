#pragma once
#include "core/inc.h"

namespace core
{
	class CommandLine
	{
	public:
		CommandLine();
		~CommandLine();

		static CommandLine& Get() ;

		void SetCommandLine(int argc, wchar_t** wargs);
		bool GetName(const std::string& name);
		bool GetInteger(const std::string& name, int& value);
		bool GetReal(const std::string& name, float& value);
		bool GetString(const std::string& name, std::string& value);

	protected:

		std::map<std::string, std::string> _CommandMap;
	};
}