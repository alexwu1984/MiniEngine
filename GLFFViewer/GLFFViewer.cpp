#include "win/win32.h"
#include "math/vector2.h"
#include "core/commandline.h"
#include "GltfViewerApp.h"
#include "Engine/ComErrorLog.h"
#include <shellapi.h>
#include <combaseapi.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	int argc = 0;
	LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
	HRESULT hr = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	int ret = 0;
	try
	{
		GltfViewApp App;
		if (App.Main(hInstance, argc, argv))
			App.Run();
	}
	catch (const _com_error& e)
	{
		Engine::LogComErrorToEngineLog(L"wWinMain", e);
		ret = 1;
	}
	catch (const std::exception& e)
	{
		Engine::LogStdExceptionToEngineLog(L"wWinMain", e);
		ret = 1;
	}
	catch (...)
	{
		Engine::LogUnknownExceptionToEngineLog(L"wWinMain");
		ret = 1;
	}
	::CoUninitialize();
	return ret;
}