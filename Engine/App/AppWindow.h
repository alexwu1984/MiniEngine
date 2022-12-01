#pragma once
#include "win/win32.h"

namespace Engine
{
	struct AppWindowP;

	class AppWindow
	{
	public:
		AppWindow(HINSTANCE hInst);
		~AppWindow();

		bool CreateAppWindow(int32_t width, int32_t height);
		int64_t WndProc(void* pWnd, uint32_t message, uint64_t wParam, int64_t lParam);
		int32_t RunLoop();
		HWND GetWnd() const;

	private:
		std::shared_ptr< AppWindowP> Data;
	};
}