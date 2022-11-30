#include "App/WindowsApp.h"
#include "core/commandline.h"

namespace Engine
{
	struct WindowApplicationP
	{
		std::shared_ptr< core::CommandLine >_CmdLine;
		HINSTANCE _hInst = nullptr;
		HWND _hWnd = nullptr;
		void* _proc_old = nullptr;
	};

	static const wchar_t WINDOW_PROP_THIS_PTR[] = L"C8C8BD2D-46A7-4DFB-BB5D-EE6A25E83368";

	static LRESULT CALLBACK ApplicationWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		WindowApplication* player = static_cast<WindowApplication*>(GetPropW(hWnd, WINDOW_PROP_THIS_PTR));
		if (player)
			return player->WndProc(reinterpret_cast<void*>(hWnd), message, wParam, lParam);
		else
			return CallWindowProcW(DefWindowProcW, hWnd, message, wParam, lParam);
	}

	WindowApplication::WindowApplication()
		:Data(std::make_shared<WindowApplicationP>())
	{

	}

	WindowApplication::~WindowApplication()
	{
		Data = {};
	}

	bool WindowApplication::Main(HINSTANCE hInst, int aargs, wchar_t** arguments)
	{
		Data->_hInst = hInst;
		Data->_CmdLine = std::make_shared<core::CommandLine>(aargs, arguments);
		
		return CreateAppWindow();
	}

	int64_t WindowApplication::WndProc(void* pWnd, uint32_t message, uint64_t wParam, int64_t lParam)
	{
		switch (message)
		{
		case WM_DESTROY:
			::PostQuitMessage(0);
			break;
		}

		if (Data->_proc_old)
			return CallWindowProcW((WNDPROC)Data->_proc_old, (HWND)pWnd, message, wParam, lParam);
		else
			return CallWindowProcW(DefWindowProcW, (HWND)pWnd, message, wParam, lParam);
	}

	void WindowApplication::Run()
	{
		win32::runLoop();
	}

	bool WindowApplication::CreateAppWindow()
	{
		WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW) };
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = ApplicationWndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = Data->_hInst;
		wcex.hIcon = NULL;
		wcex.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = NULL;
		wcex.lpszClassName = L"EngineAppWindow";
		wcex.hIconSm = NULL;

		RegisterClassExW(&wcex);

		int32_t DefWidth = 1920;
		Data->_CmdLine->GetInteger("width", DefWidth);
		int32_t DefHeight = 1080;
		Data->_CmdLine->GetInteger("height", DefHeight);

		int32_t ScreenX = ::GetSystemMetrics(SM_CXSCREEN);
		int32_t ScreenY = ::GetSystemMetrics(SM_CYSCREEN);

		Data->_hWnd = ::CreateWindowExW(0, L"EngineAppWindow", L"MiniEngine", WS_TILEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, DefWidth, DefHeight, nullptr, nullptr, Data->_hInst, nullptr);
		if (IsWindow(Data->_hWnd))
		{
			::SetPropW(Data->_hWnd, WINDOW_PROP_THIS_PTR, (void*)(WindowApplication*)this);

			SetWindowPos(Data->_hWnd, HWND_TOP, (ScreenX - DefWidth) / 2, (ScreenY - DefHeight)/2, DefWidth, DefHeight, SWP_SHOWWINDOW);
		}
		
		return true;
	}

}
