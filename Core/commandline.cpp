#include "core/commandline.h"
#include "core/logger.h"
#include "core/strings.h"
#include <sstream>

namespace core
{
	namespace
	{
		void StripLeadingHyphens(std::wstring& Key)
		{
			while (!Key.empty() && Key[0] == L'-')
				Key.erase(0, 1);
		}
	} // namespace


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
			const wchar_t* Arg = wargs ? wargs[iarg] : nullptr;
			if (!Arg || !Arg[0])
				continue;

			const bool bLeadingDash = (Arg[0] == L'-');
			const wchar_t* warg = bLeadingDash ? Arg + 1 : Arg;

			auto pwequal = wcsstr(warg, L"=");
			if (pwequal)
			{
				std::wstring KeyWide(warg, (size_t)(pwequal - warg));
				StripLeadingHyphens(KeyWide);
				std::string name = core::ucs2_u8(KeyWide);
				std::string value = core::ucs2_u8(pwequal + 1);
				if (!name.empty())
					_CommandMap[name] = value;
			}
			else if (bLeadingDash && warg[0])
			{
				// -flag without "=value" behaves as flag=1 (historically only key=value was parsed and bare flags were ignored).
				std::string name = core::ucs2_u8(warg);
				_CommandMap[name] = "1";
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
			return false;
		const std::string& S = _CommandMap[name];
		if (S.empty())
			return false;
		std::stringstream ss(S);
		int Parsed = 0;
		ss >> Parsed;
		if (ss.fail())
			return false;
		ss >> std::ws;
		if (!ss.eof())
			return false;
		value = Parsed;
		return true;
	}

	bool CommandLine::GetReal(const std::string& name, float& value)
	{
		if (!_CommandMap.contains(name))
			return false;
		const std::string& S = _CommandMap[name];
		if (S.empty())
			return false;
		std::stringstream ss(S);
		float Parsed = 0.f;
		ss >> Parsed;
		if (ss.fail())
			return false;
		ss >> std::ws;
		if (!ss.eof())
			return false;
		value = Parsed;
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