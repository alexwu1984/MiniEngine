#include "core/event.h"

namespace win32
{
	struct HighPrecisionTickPrivate;

	class HighPrecisionTick
	{
	public:
		enum class ThreadPriority
		{
			Highest,
			Normal,
			BelowNormal,
		};
	public:
		HighPrecisionTick();
		~HighPrecisionTick();

		core::event<void(float Delta)> SigTick;

		void Start(const std::string& Name, int32_t Fps, ThreadPriority Priority = ThreadPriority::Normal);
		void Stop();

	private:
		void InnerThread();
	private:
		HighPrecisionTickPrivate* Impl;
	};

	/** Rolling average over ~5s wall clock; call from the game tick thread. */
	void RecordGameThreadTickForFpsLog();

	/** Rolling average over ~5s; call once per frame at viewport present boundary (e.g. RHIEndDrawing). */
	void RecordPresentFrameForFpsLog();
}