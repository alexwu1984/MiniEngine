#pragma once

namespace RenderCore
{
	/**
	 * Host-provided draining of the recording FIFO before RHI GpuIdle ("full" sync).
	 * Engine implements via FlushRenderingCommands; thin demos omit (nullptr policy).
	 */
	class IRecordingQueueDrain
	{
	public:
		virtual ~IRecordingQueueDrain() = default;
		virtual void DrainRecordingBeforeGpuIdle() = 0;
	};
}
