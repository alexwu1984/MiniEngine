#include "Scene/WorldDirectionalShadowCSMSettings.h"
#include "Render/Shadow/FDirectionalShadowFrustumFitter.h"
#include <algorithm>

namespace Engine
{
	void WorldDirectionalShadowCSMSettings::SetShowUi(bool b)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		bShowUi = b;
	}

	bool WorldDirectionalShadowCSMSettings::GetShowUi() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return bShowUi;
	}

	void WorldDirectionalShadowCSMSettings::SetEnabled(bool b)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		bEnabled = b;
	}

	bool WorldDirectionalShadowCSMSettings::GetEnabled() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return bEnabled;
	}

	void WorldDirectionalShadowCSMSettings::SetCascadeCount(int32_t n)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		cascadeCount = (std::clamp)(n, 2, FDirectionalShadowFrustumFitter::kMaxDirectionalCascades);
	}

	int32_t WorldDirectionalShadowCSMSettings::GetCascadeCount() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return cascadeCount;
	}

	void WorldDirectionalShadowCSMSettings::SetSplit0(float v)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		split0 = (std::clamp)(v, FDirectionalShadowFrustumFitter::kCascadeSplitNormMin, FDirectionalShadowFrustumFitter::kCascadeSplitNormMax);
	}

	float WorldDirectionalShadowCSMSettings::GetSplit0() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return split0;
	}

	void WorldDirectionalShadowCSMSettings::SetSplit1(float v)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		const float lo = split0 + FDirectionalShadowFrustumFitter::kCascadeSplitPairMinGap;
		split1 = (std::clamp)(v, lo, FDirectionalShadowFrustumFitter::kCascadeSplitNormMax);
	}

	float WorldDirectionalShadowCSMSettings::GetSplit1() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return split1;
	}

	void WorldDirectionalShadowCSMSettings::ResetToDefaults()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		bShowUi = false;
		bEnabled = false;
		cascadeCount = 3;
		split0 = 0.008f;
		split1 = 0.028f;
	}
}
