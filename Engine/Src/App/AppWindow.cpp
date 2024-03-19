#include "App/AppWindow.h"
#include "Imgui/imgui_impl_win32.h"

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
		:Impl(std::make_shared<AppWindowP>())
	{
		Impl->_hInst = hInst;
	}

	AppWindow::~AppWindow()
	{

	}

	bool AppWindow::CreateAppWindow(int32_t width, int32_t height)
	{
		int32_t ScreenX = ::GetSystemMetrics(SM_CXSCREEN);
		int32_t ScreenY = ::GetSystemMetrics(SM_CYSCREEN);

		if (width > ScreenX )
		{
			width = 1280;
			height = 720;
		}

		Impl->_width = width;
		Impl->_height = height;
		WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW) };
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = ApplicationWndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = Impl->_hInst;
		wcex.hIcon = nullptr;
		wcex.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = L"EngineAppWindow";
		wcex.hIconSm = nullptr;

		RegisterClassExW(&wcex);


		Impl->_hWnd = ::CreateWindowExW(0, L"EngineAppWindow", L"MiniEngine", WS_TILEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, Impl->_hInst, nullptr);
		if (IsWindow(Impl->_hWnd))
		{
			::SetPropW(Impl->_hWnd, WINDOW_PROP_THIS_PTR, (void*)(AppWindow*)this);
			::SetWindowPos(Impl->_hWnd, HWND_TOP, (ScreenX - width) / 2, (ScreenY - height) / 2, width, height, SWP_SHOWWINDOW);
			return true;
		}
		return false;
	}

	// There is no distinct VK_xxx for keypad enter, instead it is VK_RETURN + KF_EXTENDED, we assign it an arbitrary value to make code more readable (VK_ codes go up to 255)
#define IM_VK_KEYPAD_ENTER      (VK_RETURN + 256)

	int64_t AppWindow::WndProc(void* pWnd, uint32_t message, uint64_t wParam, int64_t lParam)
	{
		ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(pWnd), message, wParam, lParam);

		switch (message)
		{
		case WM_DESTROY:
			::PostQuitMessage(0);
		break;
		case WM_SIZE:
		{
			int32_t NewWidth = LOWORD(lParam);
			int32_t NewHeight = HIWORD(lParam);
			Impl->_width = NewWidth;
			Impl->_height = NewHeight;
			EvtSizeChanged(core::vec2i(NewWidth,NewHeight));
		}
		break;
		case WM_LBUTTONDOWN:
		{
			core::vec2f Point;
			Point.x = LOWORD(lParam);
			Point.y = HIWORD(lParam);
			EvtMouseButtonDown(MouseButton::LeftButton, Point);
			::SetCapture((HWND)pWnd);
		}
		break;
		case WM_LBUTTONUP:
		{
			core::vec2f Point;
			Point.x = LOWORD(lParam);
			Point.y = HIWORD(lParam);
			EvtMouseButtonUp(MouseButton::LeftButton, Point);
			::ReleaseCapture();
		}
		break;
		case WM_RBUTTONDOWN:
		{
			core::vec2f Point;
			Point.x = LOWORD(lParam);
			Point.y = HIWORD(lParam);
			EvtMouseButtonDown(MouseButton::RightButton, Point);
		}
		break;
		case WM_RBUTTONUP:
		{
			core::vec2f Point;
			Point.x = LOWORD(lParam);
			Point.y = HIWORD(lParam);
			EvtMouseButtonUp(MouseButton::RightButton, Point);
		}
		break;
		case WM_MOUSEMOVE:
		{
			core::vec2f Point;
			Point.x = LOWORD(lParam);
			Point.y = HIWORD(lParam);

			MouseButton Button = NoButton;
			if (wParam & MK_LBUTTON)
			{
				Button = LeftButton;
			}
			else if (wParam & MK_RBUTTON)
			{
				Button = RightButton;
			}
			EvtMouseMove(Button, Point);
		}
		break;
		case WM_MOUSEWHEEL:
		{
			int32_t WheelValue = GET_WHEEL_DELTA_WPARAM(wParam);
			EvtMouseWheel(WheelValue);
		}
		break;
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		{
			const bool is_key_down = (message == WM_KEYDOWN || message == WM_SYSKEYDOWN);
			if (wParam < 256)
			{
				int vk = (int)wParam;
				if ((wParam == VK_RETURN) && (HIWORD(lParam) & KF_EXTENDED))
					vk = IM_VK_KEYPAD_ENTER;
				const int scancode = (int)LOBYTE(HIWORD(lParam));
				EvtKeyEvent(is_key_down, vk, scancode);
			}
		}
			break;
		}

		if (Impl->_proc_old)
		{
			return CallWindowProcW((WNDPROC)Impl->_proc_old, (HWND)pWnd, message, wParam, lParam);
		}
		else
		{
			return CallWindowProcW(DefWindowProcW, (HWND)pWnd, message, wParam, lParam);
		}
	}

	int32_t AppWindow::RunLoop()
	{
		MSG msg = {};
		while (msg.message != WM_QUIT)
		{
			DWORD dwWait = MsgWaitForMultipleObjectsEx(0, nullptr, 0, QS_ALLINPUT, MWMO_ALERTABLE);

			if (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				if (msg.message == WM_QUIT)
				{
					break;
				}
				::TranslateMessage(&msg);
				::DispatchMessageW(&msg);
			}
			Idle();
			

		}
		return static_cast<int32_t>(msg.wParam);
	}

	HWND AppWindow::GetWnd() const
	{
		return Impl->_hWnd;
	}

	int32_t AppWindow::GetWidth() const
	{
		return Impl->_width;
	}

	int32_t AppWindow::GetHeight() const
	{
		return Impl->_height;
	}

}