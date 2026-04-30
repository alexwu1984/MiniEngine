#pragma once
#include "Scene/Component.h"
#include "Render/MaterialPreFrame.h"
#include <cstdint>

namespace Engine
{
	/**
	 * Movable directional light (UE DirectionalLight analogue).
	 * Fills Engine::Light with Type_Directional; shadow pass fills LightView / LightViewProj for index 0.
	 */
	class DirectionalLightComponent : public Component
	{
	public:
		DECLARE_COMPONENT_CLASS_NAME(DirectionalLightComponent)
		DirectionalLightComponent(std::weak_ptr<Actor> Owner);
		~DirectionalLightComponent() override = default;

		void SetEnabled(bool bInEnabled);
		bool IsEnabled() const { return bEnabled; }

		void SetSortPriority(int32_t InPriority);
		int32_t GetSortPriority() const { return SortPriority; }

		void SetColor(const math::Vector3& InColor);
		const math::Vector3& GetColor() const { return Color; }

		void SetIntensity(float InIntensity);
		float GetIntensity() const { return Intensity; }

		void SetDepthBias(float InBias);
		float GetDepthBias() const { return DepthBias; }

		/** When true, Direction = -Owner->GetForward() (light rays along +actor forward). */
		void SetUseActorForward(bool bIn);
		bool GetUseActorForward() const { return bUseActorForward; }

		/** World-space direction toward light source (used when not bUseActorForward). Must match JSON LightDir convention. */
		void SetWorldDirection(const math::Vector3& InDir);
		const math::Vector3& GetWorldDirection() const { return WorldDirection; }

		Light BuildLight() const;

	private:
		bool bEnabled = true;
		int32_t SortPriority = 0;
		math::Vector3 Color{ 1.f, 1.f, 1.f };
		float Intensity = 1.f;
		float DepthBias = 0.f;
		bool bUseActorForward = true;
		math::Vector3 WorldDirection{ 0.f, 1.f, 0.f };
	};
	DECLARE_COMPONENT_TRAITS_CLASS_NAME(DirectionalLightComponent);
}
