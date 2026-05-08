#pragma once
#include "Scene/Component.h"
#include "Render/MaterialPreFrame.h"

namespace Engine
{
	/**
	 * Local spot light (UE SpotLight analogue). Deferred uses LightType_Spot + cone cosines.
	 * Shader convention: -Light.Direction is the cone axis (from lamp toward illuminated region).
	 */
	class SpotLightComponent : public Component
	{
	public:
		DECLARE_COMPONENT_CLASS_NAME(SpotLightComponent)
		SpotLightComponent(std::weak_ptr<Actor> Owner);
		~SpotLightComponent() override = default;

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

		/** Local +Z emission axis before actor rotation; overridden per frame by BuildLight using inv(world) like AMD GltfCommon when the actor matrix is valid. */
		void SetWorldForward(const math::Vector3& InForward);
		const math::Vector3& GetWorldForward() const { return WorldForward; }

		void SetInnerConeCos(float InCos);
		void SetOuterConeCos(float InCos);
		float GetInnerConeCos() const { return InnerConeCos; }
		float GetOuterConeCos() const { return OuterConeCos; }

		/** When true, GatherLightsForView repositions this spot from primary procedural skylight (glTFSample-style sun fill). */
		void SetProceduralSunFill(bool bIn) { bProceduralSunFill = bIn; }
		bool IsProceduralSunFill() const { return bProceduralSunFill; }

		/** Aim (world) and distance along sun ray from aim toward source; used only when IsProceduralSunFill(). */
		void SetProceduralPlacement(float sunDistanceAlongRay, const math::Vector3& aimWorld);
		float GetProceduralSunDistanceAlongRay() const { return ProceduralSunDistanceAlongRay; }
		const math::Vector3& GetProceduralAimWorld() const { return ProceduralAimWorld; }

		void SetCastShadow(bool bIn);
		bool GetCastShadow() const { return bCastShadow; }

		Light BuildLight() const;

	private:
		bool bEnabled = true;
		int32_t SortPriority = 0;
		math::Vector3 Color{ 1.f, 1.f, 1.f };
		float Intensity = 1.f;
		float Range = 100.f;
		math::Vector3 WorldForward{ 0.f, -1.f, 0.f };
		/** Defaults match glTF KHR_lights_punctual: inner half-angle 0, outer half-angle pi/4 (cosines). */
		float InnerConeCos{ 1.f };
		float OuterConeCos{ 0.70710677f };
		bool bProceduralSunFill = false;
		float ProceduralSunDistanceAlongRay = 130.f;
		math::Vector3 ProceduralAimWorld{ 0.f, 0.f, 0.f };
		bool bCastShadow = false;
	};
	DECLARE_COMPONENT_TRAITS_CLASS_NAME(SpotLightComponent);
}
