#pragma once
#include "win/win32.h"
#include "core/event.h"

namespace Engine
{
	struct AppWindowP;

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
		int32_t GetWidth() const;
		int32_t GetHeight() const;

	public:
		core::event<void()> idle;
	private:
		std::shared_ptr< AppWindowP> Impl;
	};
}