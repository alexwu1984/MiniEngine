#include "Scene/PointLightComponent.h"
#include "Scene/Actor.h"

namespace Engine
{
	IMP_COMPONENT_CLASS_NAME(PointLightComponent)
	IMP_COMPONENT_TRAITS_CLASS_NAME(PointLightComponent)

	PointLightComponent::PointLightComponent(std::weak_ptr<Actor> Owner)
		: Component(std::move(Owner))
	{
	}

	void PointLightComponent::SetEnabled(bool bInEnabled)
	{
		bEnabled = bInEnabled;
	}

	void PointLightComponent::SetSortPriority(int32_t InPriority)
	{
		SortPriority = InPriority;
	}

	void PointLightComponent::SetColor(const math::Vector3& InColor)
	{
		Color = InColor;
	}

	void PointLightComponent::SetIntensity(float InIntensity)
	{
		Intensity = InIntensity;
	}

	void PointLightComponent::SetRange(float InRange)
	{
		Range = InRange > 0.f ? InRange : 0.f;
	}

	void PointLightComponent::SetLocalOffset(const math::Vector3& InOff)
	{
		LocalOffset = InOff;
	}

	void PointLightComponent::SetCastShadow(bool bInCastShadow)
	{
		bCastShadow = bInCastShadow;
	}

	Light PointLightComponent::BuildLight() const
	{
		Light L{};
		L.Type = LightType_Point;
		L.Color = Color;
		L.Intensity = Intensity;
		L.Range = Range;
		L.ShadowMapIndex = bCastShadow ? kPointLightCubeShadowMapIndex : -1;

		L.Position = math::Vector3{};
		if (const auto owner = GetOwner())
			L.Position = owner->GetWorldTransform().TransformPosition(LocalOffset);
		L.Direction = math::Vector3(0.f, -1.f, 0.f);
		L.LightView.Identity();
		L.LightViewProj.Identity();

		L.InnerConeCos = 0.f;
		L.OuterConeCos = 0.f;
		L.DepthBias = 0.f;
		return L;
	}
}
