#include "App/AppWindow.h"

namespace Engine
{
	struct AppWindowP
	{
		HINSTANCE _hInst = nullptr;
		HWND _hWnd = nullptr;
		int32_t _width = 0;
		int32_t _height = 0;
		void* _proc_old = nullptr;
	};

	static const wchar_t WINDOW_PROP_THIS_PTR[] = L"C8C8BD2D-46A7-4DFB-BB5D-EE6A25E83368";

	static LRESULT CALLBACK ApplicationWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		AppWindow* Window = static_cast<AppWindow*>(GetPropW(hWnd, WINDOW_PROP_THIS_PTR));
		if (Window)
		{
			return Window->WndProc(reinterpret_cast<void*>(hWnd), message, wParam, lParam);
		}
		else
		{
			return ::CallWindowProcW(DefWindowProcW, hWnd, message, wParam, lParam);
		}
			
	}

	AppWindow::AppWindow(HINSTANCE hInst)
		:Data(std::make_shared<AppWindowP>())
	{
		Data->_hInst = hInst;
	}

	AppWindow::~AppWindow()
	{

	}

	bool AppWindow::CreateAppWindow(int32_t width, int32_t height)
	{
		Data->_width = width;
		Data->_height = height;
		WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW) };
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = ApplicationWndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = Data->_hInst;
		wcex.hIcon = nullptr;
		wcex.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = L"EngineAppWindow";
		wcex.hIconSm = nullptr;

		RegisterClassExW(&wcex);

		int32_t ScreenX = ::GetSystemMetrics(SM_CXSCREEN);
		int32_t ScreenY = ::GetSystemMetrics(SM_CYSCREEN);

		Data->_hWnd = ::CreateWindowExW(0, L"EngineAppWindow", L"MiniEngine", WS_TILEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, Data->_hInst, nullptr);
		if (IsWindow(Data->_hWnd))
		{
			::SetPropW(Data->_hWnd, WINDOW_PROP_THIS_PTR, (void*)(AppWindow*)this);
			::SetWindowPos(Data->_hWnd, HWND_TOP, (ScreenX - width) / 2, (ScreenY - height) / 2, width, height, SWP_SHOWWINDOW);
			return true;
		}
		return false;
	}

	int64_t AppWindow::WndProc(void* pWnd, uint32_t message, uint64_t wParam, int64_t lParam)
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

	int32_t AppWindow::RunLoop()
	{
		MSG msg = {};
		while (msg.message != WM_QUIT)
		{
			DWORD dwWait = MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT, MWMO_ALERTABLE);

			if (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				if (msg.message == WM_QUIT)
				{
					break;
				}
				::TranslateMessage(&msg);
				::DispatchMessageW(&msg);
			}
			idle();
			std::this_thread::sleep_for(1ms);

		}
		return static_cast<int32_t>(msg.wParam);
	}

	HWND AppWindow::GetWnd() const
	{
		return Data->_hWnd;
	}

	int32_t AppWindow::GetWidth() const
	{
		return Data->_width;
	}

	int32_t AppWindow::GetHeight() const
	{
		return Data->_height;
	}

}