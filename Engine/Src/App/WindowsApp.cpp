#include "App/WindowsApp.h"
#include "App/AppWindow.h"
#include "Engine/Engine.h"
#include "core/commandline.h"
#include "core/logger.h"
#include "core/system.h"

namespace Engine
{
	struct WindowApplicationP
	{
		WindowApplicationP()
		{
			Engine = std::make_shared<MainEngine>();
		}
		std::shared_ptr< AppWindow> AppWin;
		HINSTANCE hInst = nullptr;
		std::shared_ptr<MainEngine> Engine;
	};


	WindowApplication::WindowApplication()
		:Data(std::make_shared<WindowApplicationP>())
	{
		std::wstring logPath = core::process_directory().wstring()+L"/Engine.log";
		core::global_logger::start(core::ucs2_u8(logPath), core::log_inf);
	}

	WindowApplication::~WindowApplication()
	{
		Data = {};
		core::global_logger::stop();
	}

	bool WindowApplication::Main(HINSTANCE hInst, int args, wchar_t** arguments)
	{
		Data->hInst = hInst;
		core::CommandLine::Get().SetCommandLine(args, arguments);
		Data->AppWin = std::make_shared<AppWindow>(hInst);
		if (CreateAppWindow())
		{
			Data->Engine->Init(Data->AppWin);
			return true;
		}
		return false;
	}


	int32_t WindowApplication::Run()
	{
		return Data->AppWin->RunLoop();
	}

	bool WindowApplication::CreateAppWindow()
	{
		int32_t DefWidth = 1920;
		core::CommandLine::Get().GetInteger("width", DefWidth);
		int32_t DefHeight = 1080;
		core::CommandLine::Get().GetInteger("height", DefHeight);


		return Data->AppWin->CreateAppWindow(DefWidth,DefHeight);
	}

}
