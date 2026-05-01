#pragma once
#include "Engine/Thread/EngineThread.h"

namespace RenderCore
{
	class DynamicRHI;
}

namespace Engine
{
	struct RenderThreadPrivate;

	/**
	 * Executes queued lambdas that record/submit GPU work. Registers the current std::thread::id with
	 * RenderCore::RHI_RegisterRHIRecordingThread for D3D12 recording checks; GPU submit and deferred Create* run on RHISubmissionThread (RHI execution thread).
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
		RenderThreadPrivate* d_ptr = nullptr;
	};

	extern RenderThread* GRenderThread;


	/** If the lambda reads mutable state filled on the calling thread just before enqueueing, pass wait=true so that state is not overwritten until the lambda runs. */
	void ENQUEUE_UNIQUE_RENDER_COMMAND(std::function<void(RenderCore::DynamicRHI*)> fun, bool wait = false);
}