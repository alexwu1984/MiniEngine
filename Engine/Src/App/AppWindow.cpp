#include "App/AppWindow.h"
#include "App/MiniEngineWinResources.h"
#include "Engine/ComErrorLog.h"
#include "Imgui/imgui_impl_win32.h"
#include "RHI/DynamicRHI.h"
#include "core/commandline.h"

namespace Engine
{

	struct AppWindowPrivate
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

	static core::vec2f GetMousePointFromLParam(int64_t lParam)
	{
		core::vec2f Point;
		Point.x = static_cast<float>(static_cast<int16_t>(LOWORD(lParam)));
		Point.y = static_cast<float>(static_cast<int16_t>(HIWORD(lParam)));
		return Point;
	}

	static void ReleaseMouseCaptureIfNoButtons(void* pWnd, uint64_t wParam)
	{
		if (!(wParam & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)) && ::GetCapture() == static_cast<HWND>(pWnd))
			::ReleaseCapture();
	}

	static MouseButton GetPressedMouseButtonFromMessage(uint64_t wParam)
	{
		if ((wParam & MK_LBUTTON) || (::GetKeyState(VK_LBUTTON) & 0x8000))
			return MouseButton::LeftButton;
		if ((wParam & MK_RBUTTON) || (::GetKeyState(VK_RBUTTON) & 0x8000))
			return MouseButton::RightButton;
		return MouseButton::NoButton;
	}

	AppWindow::AppWindow(HINSTANCE hInst)
		:d_ptr(new AppWindowPrivate())
	{
		C_P(AppWindow);
		d->_hInst = hInst;
	}

	AppWindow::~AppWindow()
	{
		delete d_ptr;
	}

	bool AppWindow::CreateAppWindow(int32_t width, int32_t height)
	{
		C_P(AppWindow);
		int32_t ScreenX = ::GetSystemMetrics(SM_CXSCREEN);
		int32_t ScreenY = ::GetSystemMetrics(SM_CYSCREEN);

		if (width > ScreenX )
		{
			width = 1280;
			height = 720;
		}

		d->_width = width;
		d->_height = height;
		WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW) };
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = ApplicationWndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = d->_hInst;
		{
			const HICON iconLg = static_cast<HICON>(::LoadImageW(
				d->_hInst,
				MAKEINTRESOURCEW(MINIENGINE_ICON_RESOURCE_ID),
				IMAGE_ICON,
				::GetSystemMetrics(SM_CXICON),
				::GetSystemMetrics(SM_CYICON),
				LR_DEFAULTCOLOR));
			const HICON iconSm = static_cast<HICON>(::LoadImageW(
				d->_hInst,
				MAKEINTRESOURCEW(MINIENGINE_ICON_RESOURCE_ID),
				IMAGE_ICON,
				::GetSystemMetrics(SM_CXSMICON),
				::GetSystemMetrics(SM_CYSMICON),
				LR_DEFAULTCOLOR));
			wcex.hIcon = iconLg ? iconLg : ::LoadIcon(nullptr, IDI_APPLICATION);
			wcex.hIconSm = iconSm ? iconSm : wcex.hIcon;
		}
		wcex.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = L"EngineAppWindow";

		RegisterClassExW(&wcex);

		d->_hWnd = ::CreateWindowExW(0, L"EngineAppWindow", L"MiniEngine", WS_TILEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, d->_hInst, nullptr);
		if (IsWindow(d->_hWnd))
		{
			::SetPropW(d->_hWnd, WINDOW_PROP_THIS_PTR, (void*)(AppWindow*)this);
			::SetWindowPos(d->_hWnd, HWND_TOP, (ScreenX - width) / 2, (ScreenY - height) / 2, width, height, SWP_SHOWWINDOW);
			return true;
		}
		return false;
	}

	// There is no distinct VK_xxx for keypad enter, instead it is VK_RETURN + KF_EXTENDED, we assign it an arbitrary value to make code more readable (VK_ codes go up to 255)
#define IM_VK_KEYPAD_ENTER      (VK_RETURN + 256)

	int64_t AppWindow::WndProc(void* pWnd, uint32_t message, uint64_t wParam, int64_t lParam)
	{
		C_P(AppWindow);
		if (!core::CommandLine::Get().GetName("noimgui"))
		{
			auto ImguiRet = ::ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(pWnd), message, wParam, lParam);
			if (ImguiRet)
				return ImguiRet;
		}
		
		if (ImGui::GetCurrentContext())
		{
			const ImGuiIO& io = ImGui::GetIO();
			if (io.WantCaptureMouse && (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || 
										message == WM_RBUTTONDOWN || message == WM_RBUTTONUP || 
										message == WM_MBUTTONDOWN || message == WM_MBUTTONUP ||
										message == WM_MOUSEWHEEL || message == WM_MOUSEMOVE)) {
				return ERROR_SUCCESS;
			}
		}


		switch (message)
		{
		case WM_DESTROY:
			::PostQuitMessage(0);
		break;
		case WM_SIZE:
		{
			int32_t NewWidth = LOWORD(lParam);
			int32_t NewHeight = HIWORD(lParam);
			d->_width = NewWidth;
			d->_height = NewHeight;
			EvtSizeChanged(core::vec2i(NewWidth,NewHeight));
		}
		break;
		case WM_LBUTTONDOWN:
		{
			core::vec2f Point = GetMousePointFromLParam(lParam);
			EvtMouseButtonDown(MouseButton::LeftButton, Point);
			::SetCapture((HWND)pWnd);
		}
		break;
		case WM_LBUTTONUP:
		{
			core::vec2f Point = GetMousePointFromLParam(lParam);
			EvtMouseButtonUp(MouseButton::LeftButton, Point);
			ReleaseMouseCaptureIfNoButtons(pWnd, wParam);
		}
		break;
		case WM_RBUTTONDOWN:
		{
			core::vec2f Point = GetMousePointFromLParam(lParam);
			EvtMouseButtonDown(MouseButton::RightButton, Point);
			::SetCapture((HWND)pWnd);
		}
		break;
		case WM_RBUTTONUP:
		{
			core::vec2f Point = GetMousePointFromLParam(lParam);
			EvtMouseButtonUp(MouseButton::RightButton, Point);
			ReleaseMouseCaptureIfNoButtons(pWnd, wParam);
		}
		break;
		case WM_MOUSEMOVE:
		{
			core::vec2f Point = GetMousePointFromLParam(lParam);
			MouseButton Button = GetPressedMouseButtonFromMessage(wParam);
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

		if (d->_proc_old)
		{
			return CallWindowProcW((WNDPROC)d->_proc_old, (HWND)pWnd, message, wParam, lParam);
		}
		else
		{
			return CallWindowProcW(DefWindowProcW, (HWND)pWnd, message, wParam, lParam);
		}
	}

	int32_t AppWindow::RunLoop()
	{
		C_P(AppWindow);
		MSG msg = {};
		while (msg.message != WM_QUIT)
		{
			try
			{
				(void)MsgWaitForMultipleObjectsEx(0, nullptr, 0, QS_ALLINPUT, MWMO_ALERTABLE);

				const BOOL hadWinMsg = ::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE);
				if (hadWinMsg)
				{
					if (msg.message == WM_QUIT)
						break;
					::TranslateMessage(&msg);
					::DispatchMessageW(&msg);
				}
				else
				{
					// Avoid a pure busy-spin (MsgWait timeout 0 + Idle/present): yield so driver & worker threads schedule.
					::Sleep(0);
				}

				// After GPU fatal loss we no longer present; spinning Idle() only shows a blank client area.
				// Exit the loop so WindowApplication::Run reaches ShutDown() (stops game tick + RHI teardown).
				if (RenderCore::RHI_HasFatalDeviceLossForShell())
					break;

				Idle();
			}
			catch (const _com_error& e)
			{
				LogComErrorToEngineLog(L"AppWindow::RunLoop(main_thread)", e);
				break;
			}
			catch (const std::exception& e)
			{
				LogStdExceptionToEngineLog(L"AppWindow::RunLoop(main_thread)", e);
				break;
			}
			catch (...)
			{
				LogUnknownExceptionToEngineLog(L"AppWindow::RunLoop(main_thread)");
				break;
			}
		}
		return static_cast<int32_t>(msg.wParam);
	}

	HWND AppWindow::GetWnd() const
	{
		C_P(const AppWindow);
		return d->_hWnd;
	}

	int32_t AppWindow::GetWidth() const
	{
		C_P(const AppWindow);
		return d->_width;
	}

	int32_t AppWindow::GetHeight() const
	{
		C_P(const AppWindow);
		return d->_height;
	}

}