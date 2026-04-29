#pragma once
#include "Engine/Thread/EngineThread.h"

namespace RenderCore
{
	class DynamicRHI;
}

namespace Engine
{
	struct RenderThreadP;

	/**
	 * Executes queued lambdas that record/submit GPU work. Registers the current std::thread::id with
	 * RenderCore::RHI_RegisterRHIRecordingThread for D3D12 recording checks; GPU submit may run on RHISubmissionThread.
	 */
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


	void ENQUEUE_UNIQUE_RENDER_COMMAND(std::function<void(RenderCore::DynamicRHI*)> fun,bool wait = false);
}