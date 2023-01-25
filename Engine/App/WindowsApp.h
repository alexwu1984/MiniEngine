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
		void Run();
		virtual bool Init() { return true; }
		virtual void ShutDown() {};
	private:
		bool CreateAppWindow();

	private:
		std::shared_ptr<WindowApplicationP> Data;
	};
}