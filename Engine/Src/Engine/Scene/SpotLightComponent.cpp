#include "Scene/SpotLightComponent.h"
#include "Scene/Actor.h"
#include "math/vector4.h"

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

		L.Position = math::Vector3{};
		const auto owner = GetOwner();
		if (owner)
			L.Position = owner->GetPosition();

		// AMD GltfCommon::SetPerFrameData: direction from transpose(lightView) * (0,0,1,0) with lightView = inv(light world mat).
		// Row-vector engine: (0,0,1,0) * lightView equals the same 3-vector (see glTFSample math layout).
		if (owner)
		{
			const math::Matrix4x4& W = owner->GetWorldTransform();
			const math::Matrix4x4 lightView = W.Inverse();
			const math::Vector4 d4 = math::Vector4(0.f, 0.f, 1.f, 0.f) * lightView;
			math::Vector3 dir(d4.x, d4.y, d4.z);
			if (dir.GetSqrLength() > 1e-14f)
				L.Direction = dir.Normalize();
			else
			{
				math::Vector3 fwd = WorldForward;
				const math::Vector3 localF = fwd;
				const math::Vector3 row0(W._00, W._01, W._02);
				const math::Vector3 row1(W._10, W._11, W._12);
				const math::Vector3 row2(W._20, W._21, W._22);
				fwd = math::Vector3(row0.Dot(localF), row1.Dot(localF), row2.Dot(localF)).Normalize();
				if (fwd.GetSqrLength() < 1e-10f)
					fwd = math::Vector3(0.f, -1.f, 0.f);
				L.Direction = -fwd;
			}
		}
		else
		{
			math::Vector3 fwd = WorldForward.Normalize();
			if (fwd.GetSqrLength() < 1e-10f)
				fwd = math::Vector3(0.f, -1.f, 0.f);
			L.Direction = -fwd;
		}

		L.InnerConeCos = InnerConeCos;
		L.OuterConeCos = OuterConeCos;
		return L;
	}
}
