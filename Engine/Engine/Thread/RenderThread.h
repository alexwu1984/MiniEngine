#pragma once
#include "Engine/Thread/EngineThread.h"

namespace Engine
{
	struct RenderThreadP;
	class RenderThread
	{
	public:
		RenderThread();
		~RenderThread();

		void Start();
		void Stop();

		void AppendCommand(std::function<void()> fun);

	private:
		void Run();

	private:
		std::unique_ptr<RenderThreadP> Data;
	};

	extern RenderThread* GRenderThread;

#define ENQUEUE_UNIQUE_RENDER_COMMAND(Command) \
	if(GRenderThread ) \
	{ \
		GRenderThread->AppendCommand(Command); \
	}

}