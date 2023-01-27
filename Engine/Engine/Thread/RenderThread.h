#pragma once
#include "Engine/Thread/EngineThread.h"

namespace RenderCore
{
	class DynamicRHI;
}

namespace Engine
{
	
	struct RenderThreadP;
	class RenderThread
	{
	public:
		RenderThread(RenderCore::DynamicRHI* DyRHI);
		~RenderThread();

		void Start();
		void Stop();

		void AppendCommand(std::function<void(RenderCore::DynamicRHI*)> fun);
		void WaitForFinish();

	private:
		void Run();

	private:
		std::unique_ptr<RenderThreadP> Impl;
	};

	extern RenderThread* GRenderThread;

#define ENQUEUE_UNIQUE_RENDER_COMMAND(Command) \
	if(GRenderThread ) \
	{ \
		GRenderThread->AppendCommand(Command); \
	}

}