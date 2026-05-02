#pragma once
#include "Scene/Component.h"
#include "Render/MaterialPreFrame.h"
#include <cstdint>

namespace Engine
{
	/**
	 * Local punctual light (UE PointLight analogue). World Position = Owner world translation + LocalOffset.
	 * Feeds deferred + forward analytic branch (LightType_Point); no shadow in this tutorial path.
	 */
	class PointLightComponent : public Component
	{
	public:
		DECLARE_COMPONENT_CLASS_NAME(PointLightComponent)
		PointLightComponent(std::weak_ptr<Actor> Owner);
		~PointLightComponent() override = default;

		void SetEnabled(bool bInEnabled);
		bool IsEnabled() const { return bEnabled; }

		void SetSortPriority(int32_t InPriority);
		int32_t GetSortPriority() const { return SortPriority; }

		void SetColor(const math::Vector3& InColor);
		const math::Vector3& GetColor() const { return Color; }

		void SetIntensity(float InIntensity);
		float GetIntensity() const { return Intensity; }

		void SetRange(float InRange);
		float GetRange() const { return Range; }

		/** Extra offset in actor local space before world transform translation (typically zero). */
		void SetLocalOffset(const math::Vector3& InOff);
		const math::Vector3& GetLocalOffset() const { return LocalOffset; }

		Light BuildLight() const;

	private:
		bool bEnabled = true;
		int32_t SortPriority = 0;
		math::Vector3 Color{ 1.f, 1.f, 1.f };
		float Intensity = 1.f;
		float Range = 10.f;
		math::Vector3 LocalOffset{};
	};
	DECLARE_COMPONENT_TRAITS_CLASS_NAME(PointLightComponent);
}
