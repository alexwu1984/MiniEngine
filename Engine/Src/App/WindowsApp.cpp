#include "App/WindowsApp.h"
#include "App/AppWindow.h"
#include "core/commandline.h"


namespace Engine
{
	struct WindowApplicationP
	{

		std::shared_ptr< core::CommandLine >_CmdLine;
		std::shared_ptr< AppWindow> _AppWindow;
		HINSTANCE _hInst = nullptr;
		
	};


	WindowApplication::WindowApplication()
		:Data(std::make_shared<WindowApplicationP>())
	{

	}

	WindowApplication::~WindowApplication()
	{
		Data = {};
	}

	bool WindowApplication::Main(HINSTANCE hInst, int args, wchar_t** arguments)
	{
		Data->_hInst = hInst;
		Data->_CmdLine = std::make_shared<core::CommandLine>(args, arguments);
		Data->_AppWindow = std::make_shared<AppWindow>(hInst);
		return CreateAppWindow();
	}


	int32_t WindowApplication::Run()
	{
		return Data->_AppWindow->RunLoop();
	}

	bool WindowApplication::CreateAppWindow()
	{
		int32_t DefWidth = 1920;
		Data->_CmdLine->GetInteger("width", DefWidth);
		int32_t DefHeight = 1080;
		Data->_CmdLine->GetInteger("height", DefHeight);


		return Data->_AppWindow->CreateAppWindow(DefWidth,DefHeight);
	}

}
