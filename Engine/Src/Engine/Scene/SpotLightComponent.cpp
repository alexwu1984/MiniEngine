#include "Scene/SpotLightComponent.h"
#include "Scene/Actor.h"

namespace Engine
{
	IMP_COMPONENT_CLASS_NAME(SpotLightComponent)
	IMP_COMPONENT_TRAITS_CLASS_NAME(SpotLightComponent)

	SpotLightComponent::SpotLightComponent(std::weak_ptr<Actor> Owner)
		: Component(std::move(Owner))
	{
	}

	void SpotLightComponent::SetEnabled(bool bInEnabled)
	{
		bEnabled = bInEnabled;
	}

	void SpotLightComponent::SetSortPriority(int32_t InPriority)
	{
		SortPriority = InPriority;
	}

	void SpotLightComponent::SetColor(const math::Vector3& InColor)
	{
		Color = InColor;
	}

	void SpotLightComponent::SetIntensity(float InIntensity)
	{
		Intensity = InIntensity;
	}

	void SpotLightComponent::SetRange(float InRange)
	{
		Range = InRange > 0.f ? InRange : 0.f;
	}

	void SpotLightComponent::SetWorldForward(const math::Vector3& InForward)
	{
		WorldForward = InForward;
	}

	void SpotLightComponent::SetProceduralPlacement(float sunDistanceAlongRay, const math::Vector3& aimWorld)
	{
		ProceduralSunDistanceAlongRay = sunDistanceAlongRay > 0.f ? sunDistanceAlongRay : 1.f;
		ProceduralAimWorld = aimWorld;
	}

	void SpotLightComponent::SetCastShadow(bool bIn)
	{
		bCastShadow = bIn;
	}

	void SpotLightComponent::SetInnerConeCos(float InCos)
	{
		InnerConeCos = InCos;
	}

	void SpotLightComponent::SetOuterConeCos(float InCos)
	{
		OuterConeCos = InCos;
	}

	Light SpotLightComponent::BuildLight() const
	{
		Light L{};
		L.Type = LightType_Spot;
		L.Color = Color;
		L.Intensity = Intensity;
		L.Range = Range;
		L.ShadowMapIndex = bCastShadow ? kSpotLightShadowMapIndex : -1;
		L.DepthBias = 0.f;
		L.LightView.Identity();
		L.LightViewProj.Identity();

		math::Vector3 fwd = WorldForward;
		if (const auto owner = GetOwner())
		{
			const math::Matrix4x4& W = owner->GetWorldTransform();
			const math::Vector3 localF = fwd;
			const math::Vector3 row0(W._00, W._01, W._02);
			const math::Vector3 row1(W._10, W._11, W._12);
			const math::Vector3 row2(W._20, W._21, W._22);
			fwd = math::Vector3(row0.Dot(localF), row1.Dot(localF), row2.Dot(localF)).Normalize();
		}
		else
			fwd = fwd.Normalize();

		if (fwd.GetSqrLength() < 1e-10f)
			fwd = math::Vector3(0.f, -1.f, 0.f);

		// Deferred: SpotDirection = -L.Direction must equal forward from lamp into the scene.
		L.Direction = -fwd;
		L.Position = math::Vector3{};
		if (const auto owner = GetOwner())
			L.Position = owner->GetPosition();

		L.InnerConeCos = InnerConeCos;
		L.OuterConeCos = OuterConeCos;
		return L;
	}
}
