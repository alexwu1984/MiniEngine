#include "Scene/SkyLightComponent.h"
#include "Scene/Actor.h"
#include "core/system.h"

namespace Engine
{
	IMP_COMPONENT_CLASS_NAME(SkyLightComponent)
	IMP_COMPONENT_TRAITS_CLASS_NAME(SkyLightComponent)

	SkyLightComponent::SkyLightComponent(std::weak_ptr<Actor> Owner)
		: Component(std::move(Owner))
	{
	}

	void SkyLightComponent::SetEnabled(bool bInEnabled)
	{
		bEnabled = bInEnabled;
	}

	void SkyLightComponent::SetSortPriority(int32_t InPriority)
	{
		SortPriority = InPriority;
	}

	void SkyLightComponent::SetHDRRelativePath(std::wstring InRelativePath)
	{
		HdrRelativePath = std::move(InRelativePath);
	}

	void SkyLightComponent::SetIBLIntensity(float InIntensity)
	{
		IBLIntensity = InIntensity;
	}

	void SkyLightComponent::SetIBLRotationPitchDegrees(float InPitchDeg)
	{
		IBLRotationPitchDegrees = InPitchDeg;
	}

	void SkyLightComponent::SetIBLRotationYawDegrees(float InYawDeg)
	{
		IBLRotationYawDegrees = InYawDeg;
	}

	std::wstring SkyLightComponent::ResolveHDRFullPath() const
	{
		return core::process_directory().wstring() + L"/GLTFModel/" + HdrRelativePath;
	}

	FSkyLightSourceDesc SkyLightComponent::BuildSkyLightSourceDesc() const
	{
		FSkyLightSourceDesc out{};
		if (!bEnabled)
		{
			out.Type = ESkyLightSourceType::None;
			return out;
		}
		if (bProceduralSky)
		{
			out.Type = ESkyLightSourceType::Procedural;
			return out;
		}
		if (HdrRelativePath.empty())
		{
			out.Type = ESkyLightSourceType::None;
			return out;
		}
		const std::wstring full = ResolveHDRFullPath();
		if (full.empty())
		{
			out.Type = ESkyLightSourceType::None;
			return out;
		}
		out.Type = ESkyLightSourceType::HdrFile;
		out.HdrFileFullPath = full;
		return out;
	}
}
