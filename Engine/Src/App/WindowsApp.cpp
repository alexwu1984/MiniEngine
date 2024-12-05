#include "App/WindowsApp.h"
#include "App/AppWindow.h"
#include "Engine/Engine.h"
#include "core/commandline.h"
#include "core/logger.h"
#include "core/system.h"

namespace Engine
{
	struct WindowApplicationPrivate
	{
		WindowApplicationPrivate()
		{
			Engine = std::make_shared<MainEngine>();
		}
		std::shared_ptr< AppWindow> AppWin;
		HINSTANCE hInst = nullptr;
		std::shared_ptr<MainEngine> Engine;
	};


	WindowApplication::WindowApplication()
		:d_ptr(new WindowApplicationPrivate())
	{
		std::wstring logPath = core::process_directory().wstring()+L"/Engine.log";
		core::global_logger::start(core::ucs2_u8(logPath), core::log_inf);
	}

	WindowApplication::~WindowApplication()
	{
		delete d_ptr;
		core::global_logger::stop();
	}

	bool WindowApplication::Main(HINSTANCE hInst, int args, wchar_t** arguments)
	{
		C_P(WindowApplication);
		d->hInst = hInst;
		core::CommandLine::Get().SetCommandLine(args, arguments);
		d->AppWin = std::make_shared<AppWindow>(hInst);
		if (!CreateAppWindow())
		{
			return false;
		}
		RenderCore::RHIAPIType ApiType = RenderCore::RHIAPIType::E_D3D11;
		std::string apiStr;
		core::CommandLine::Get().GetString("render_api", apiStr);
		if (apiStr == "D3D12")
		{
			ApiType = RenderCore::RHIAPIType::E_D3D12;
		}
		d->Engine->Init(d->AppWin, ApiType);
		bool bRet =  Init();
		if (bRet)
		{
			d->Engine->StartThread();
		}
		return bRet;
	}


	void WindowApplication::Run()
	{
		C_P(WindowApplication);
		d->AppWin->RunLoop();
		ShutDown();
		d->Engine->ShutDown();
	}

	bool WindowApplication::CreateAppWindow()
	{
		C_P(WindowApplication);
		int32_t DefWidth = 1920;
		core::CommandLine::Get().GetInteger("width", DefWidth);
		int32_t DefHeight = 1080;
		core::CommandLine::Get().GetInteger("height", DefHeight);
		return d->AppWin->CreateAppWindow(DefWidth,DefHeight);
	}
}
