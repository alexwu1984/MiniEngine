#include "win/win32.h"
#include "math/vector2.h"
#include "core/commandline.h"
#include "GltfViewerApp.h"
#include <shellapi.h>
#include <combaseapi.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	int argc = 0;
	LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
	HRESULT hr = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	GltfViewApp App;
	if (App.Main(hInstance, argc, argv))
	{
		App.Run();
	}
	::CoUninitialize();
	return 0;
}