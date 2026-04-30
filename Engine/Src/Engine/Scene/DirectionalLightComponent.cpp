#include "Scene/DirectionalLightComponent.h"
#include "Scene/Actor.h"

namespace Engine
{
	IMP_COMPONENT_CLASS_NAME(DirectionalLightComponent)
	IMP_COMPONENT_TRAITS_CLASS_NAME(DirectionalLightComponent)

	DirectionalLightComponent::DirectionalLightComponent(std::weak_ptr<Actor> Owner)
		: Component(std::move(Owner))
	{
	}

	void DirectionalLightComponent::SetEnabled(bool bInEnabled)
	{
		bEnabled = bInEnabled;
	}

	void DirectionalLightComponent::SetSortPriority(int32_t InPriority)
	{
		SortPriority = InPriority;
	}

	void DirectionalLightComponent::SetColor(const math::Vector3& InColor)
	{
		Color = InColor;
	}

	void DirectionalLightComponent::SetIntensity(float InIntensity)
	{
		Intensity = InIntensity;
	}

	void DirectionalLightComponent::SetDepthBias(float InBias)
	{
		DepthBias = InBias;
	}

	void DirectionalLightComponent::SetUseActorForward(bool bIn)
	{
		bUseActorForward = bIn;
	}

	void DirectionalLightComponent::SetWorldDirection(const math::Vector3& InDir)
	{
		WorldDirection = InDir;
	}

	Light DirectionalLightComponent::BuildLight() const
	{
		Light L{};
		L.Type = LightType_Directional;
		L.Color = Color;
		L.Intensity = Intensity;
		L.DepthBias = DepthBias;
		L.ShadowMapIndex = -1;

		math::Vector3 dir;
		if (bUseActorForward)
		{
			if (const auto owner = GetOwner())
				dir = -owner->GetForward();
			else
				dir = WorldDirection;
		}
		else
			dir = WorldDirection;

		dir = dir.Normalize();
		if (dir.GetSqrLength() < 1e-8f)
			dir = math::Vector3(0.f, 1.f, 0.f);
		L.Direction = dir;
		return L;
	}
}
