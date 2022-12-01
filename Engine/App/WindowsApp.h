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
		int32_t Run();
	private:
		bool CreateAppWindow();

	private:
		std::shared_ptr<WindowApplicationP> Data;
	};
}