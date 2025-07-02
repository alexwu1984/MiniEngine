#include "win/win32.h"
#include "core/commandline.h"
#include "ViewerApp.h"
#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	int argc = 0;
	LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);

	ViewerApp App;
	if (App.Main(hInstance, argc, argv))
	{
		App.Run();
	}
	return 0;
}
