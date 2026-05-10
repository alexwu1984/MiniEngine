#pragma once
#include "Scene/Component.h"
#include "Render/SkyLightEnvironment.h"
#include "math/vector3.h"

namespace Engine
{
	/**
	 * Scene-driven skylight / IBL source (UE SkyLight analogue).
	 * HDR path is relative to GLTFModel/ (same convention as scene JSON Evn.Hdr).
	 * World picks the enabled component with highest SortPriority each frame.
	 */
	class SkyLightComponent : public Component
	{
	public:
		DECLARE_COMPONENT_CLASS_NAME(SkyLightComponent)
		SkyLightComponent(std::weak_ptr<Actor> Owner);
		~SkyLightComponent() override = default;

		void SetEnabled(bool bInEnabled);
		bool IsEnabled() const { return bEnabled; }

		void SetSortPriority(int32_t InPriority);
		int32_t GetSortPriority() const { return SortPriority; }

		void SetHDRRelativePath(std::wstring InRelativePath);
		const std::wstring& GetHDRRelativePath() const { return HdrRelativePath; }

		/** When true, IBL comes from an analytic lat-long environment (no file under GLTFModel/). */
		void SetProceduralSky(bool bInProceduralSky) { bProceduralSky = bInProceduralSky; }
		bool IsProceduralSky() const { return bProceduralSky; }

		/** Scales skylight IBL in Lit materials (UE Real-time Capture intensity analogue). */
		void SetIBLIntensity(float InIntensity);
		float GetIBLIntensity() const { return IBLIntensity; }

		/** Used when ProceduralSky is enabled: directional light + sun disk share this sun direction (world toward sun). */
		void SetProceduralSunDirectionTowardSource(const math::Vector3& InDir) { ProceduralSunDirectionTowardSource = InDir; }
		const math::Vector3& GetProceduralSunDirectionTowardSource() const { return ProceduralSunDirectionTowardSource; }

		/** Absolute path passed to RHI HDR load (process_directory/GLTFModel/ + relative). */
		std::wstring ResolveHDRFullPath() const;

		/** Convert this component into a skylight source description for rendering. */
		FSkyLightSourceDesc BuildSkyLightSourceDesc() const;

	private:
		bool bEnabled = true;
		bool bProceduralSky = false;
		int32_t SortPriority = 0;
		std::wstring HdrRelativePath;
		float IBLIntensity = 1.f;
		/** Matches glTFSample SkyDomeProc: Renderer passes vSunDirection (1, 0.05, 0); normalized when applied. */
		math::Vector3 ProceduralSunDirectionTowardSource{ 1.f, 0.05f, 0.f };
	};
	DECLARE_COMPONENT_TRAITS_CLASS_NAME(SkyLightComponent);
}
