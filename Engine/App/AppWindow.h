#pragma once
#include "win/win32.h"
#include "core/event.h"
#include "Scene/DeviceInputState.h"

namespace Engine
{
	struct AppWindowPrivate;

	class AppWindow 
	{
	public:
		AppWindow(HINSTANCE hInst);
		~AppWindow();

		AppWindow(const AppWindow&) = delete;
		AppWindow operator = (const AppWindow&) = delete;

		bool CreateAppWindow(int32_t width, int32_t height);
		int64_t WndProc(void* pWnd, uint32_t message, uint64_t wParam, int64_t lParam);
		int32_t RunLoop();
		HWND GetWnd() const;
		/** True when this top-level window is the Win32 foreground window. */
		bool IsForeground() const;
		int32_t GetWidth() const;
		int32_t GetHeight() const;

	public:
		core::event<void()> Idle;
		core::event<void(core::vec2i)> EvtSizeChanged;
		core::event<void(MouseButton Button, core::vec2f)> EvtMouseButtonDown;
		core::event<void(MouseButton Button, core::vec2f)> EvtMouseButtonUp;
		core::event<void(MouseButton Button, core::vec2f)> EvtMouseMove;
		core::event<void(int32_t WheelValue)> EvtMouseWheel;
		core::event<void(bool is_key_down, int32_t vk, int32_t scancode)> EvtKeyEvent;
	private:
		AppWindowPrivate* d_ptr = nullptr;
	};
}