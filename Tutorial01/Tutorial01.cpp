#include "win/win32.h"
#include "math/vector2.h"
#include "core/commandline.h"
#include "App/windowsapp.h"
#include "core/memory_manager.h"
#include <shellapi.h>

struct Test
{
	Test()
	{

	}

	~Test()
	{

	}
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	int argc = 0;
	LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);

	win32::stack_memory m(sizeof(Test));
	Test* address = (Test*)m.Allocate(sizeof(Test), 0, false);
	new(address) Test();
	address->~Test();
	m.PopMemory();

	address = (Test*)m.Allocate(sizeof(Test), 0, false);
	new(address) Test();
	address->~Test();
	
	Engine::WindowApplication App;
	if (App.Main(hInstance, argc, argv))
	{
		App.Run();
	}

	return 0;
}