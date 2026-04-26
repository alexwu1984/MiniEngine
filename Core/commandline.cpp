#include "core/commandline.h"
#include "core/logger.h"
#include "core/strings.h"

namespace core
{

	CommandLine::CommandLine()
	{
	
	}

	CommandLine::~CommandLine()
	{

	}

	CommandLine& CommandLine::Get()
	{
		static CommandLine CmdLIne;
		return CmdLIne;
	}

	void CommandLine::SetCommandLine(int argc, wchar_t** wargs)
	{
		core::inf() << __FUNCTION__ " " << argc << (wargs ? " wide " : " ") << " arguments";
		for (int iarg = 0; iarg < argc; ++iarg)
		{
			if (wargs)
				core::inf() << __FUNCTION__ " wide argumnets[" << iarg << "]=" << wargs[iarg];
		}

		for (int iarg = 0; iarg < argc; ++iarg)
		{
			const wchar_t* warg = wargs ? wargs[iarg] : nullptr;
			if (warg && warg[0] == L'-')
				++warg;

			auto pwequal = warg ? wcsstr(warg, L"=") : nullptr;
			if (pwequal)
			{
				std::string name = core::ucs2_u8(reinterpret_cast<const wchar_t*>(warg), static_cast<int32_t>(pwequal - warg));
				std::string value = core::ucs2_u8(pwequal + 1);
				_CommandMap[name] = value;
			}
		}
	}

	bool CommandLine::GetName(const std::string& name)
	{
		return _CommandMap.contains(name);
	}

	bool CommandLine::GetInteger(const std::string& name, int& value)
	{
		if (!_CommandMap.contains(name))
		{
			return false;
		}
		std::stringstream ss;
		ss << _CommandMap[name];
		ss >> value;
		return true;
	}

	bool CommandLine::GetReal(const std::string& name, float& value)
	{
		if (!_CommandMap.contains(name))
		{
			return false;
		}
		std::stringstream ss;
		ss << _CommandMap[name];
		ss >> value;
		return true;
	}

	bool CommandLine::GetString(const std::string& name, std::string& value)
	{
		if (!_CommandMap.contains(name))
		{
			return false;
		}
		value = _CommandMap[name];
		return true;
	}

}