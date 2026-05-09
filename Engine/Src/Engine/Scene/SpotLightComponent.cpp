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
		// Negative range = unlimited (KHR / AMD GltfCommon).
		Range = InRange;
	}

	void SpotLightComponent::SetWorldForward(const math::Vector3& InForward)
	{
		WorldForward = InForward;
	}

	void SpotLightComponent::SetConeAxisWorld(const math::Vector3& InAxis)
	{
		math::Vector3 a = InAxis;
		if (a.GetSqrLength() < 1e-10f)
			a = math::Vector3(0.f, 0.f, 1.f);
		else
			a = a.Normalize();
		ConeAxisWorld = a;
		WorldForward = a;
		if (const auto owner = GetOwner())
		{
			owner->RotateToNewForward(-a);
			owner->ComputeWorldTransform(0.f);
		}
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

		const auto owner = GetOwner();
		math::Vector3 cone = ConeAxisWorld;
		if (cone.GetSqrLength() < 1e-10f)
			cone = math::Vector3(0.f, 0.f, 1.f);
		else
			cone = cone.Normalize();
		// Deferred: SpotDirection = -L.Direction == cone (world emission). L.Direction = -cone.
		if (owner)
		{
			L.Position = owner->GetPosition();
			L.Direction = (-cone).Normalize();
		}
		else
		{
			L.Position = math::Vector3{};
			L.Direction = (-cone).Normalize();
		}

		L.InnerConeCos = InnerConeCos;
		L.OuterConeCos = OuterConeCos;
		return L;
	}
}
