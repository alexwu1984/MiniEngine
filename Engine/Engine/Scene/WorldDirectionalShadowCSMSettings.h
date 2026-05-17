#pragma once
#include "core/inc.h"

namespace Engine
{
	/** Runtime directional CSM toggles (Evn + ImGui); not part of core actor graph. */
	class WorldDirectionalShadowCSMSettings
	{
	public:
		void SetShowUi(bool b);
		bool GetShowUi() const;

		void SetEnabled(bool b);
		bool GetEnabled() const;

		void SetCascadeCount(int32_t n);
		int32_t GetCascadeCount() const;

		void SetSplit0(float v);
		float GetSplit0() const;

		void SetSplit1(float v);
		float GetSplit1() const;

		void ResetToDefaults();

	private:
		mutable std::mutex mutex_{};
		bool bShowUi = false;
		bool bEnabled = false;
		int32_t cascadeCount = 3;
		float split0 = 0.008f;
		float split1 = 0.028f;
	};
}
