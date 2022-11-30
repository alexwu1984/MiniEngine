#pragma once
#include "win/win32.h"

namespace Engine
{
	struct WindowApplicationP;

	class WindowApplication
	{
	public:
		WindowApplication();
		virtual ~WindowApplication();

		bool Main(HINSTANCE hInst, int aargs, wchar_t** arguments);
		virtual int64_t WndProc(void* pWnd, uint32_t message, uint64_t wParam, int64_t lParam);
		void Run();
	private:
		bool CreateAppWindow();

	private:
		std::shared_ptr<WindowApplicationP> Data;
	};
}