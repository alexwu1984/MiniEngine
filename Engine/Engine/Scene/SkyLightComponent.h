#pragma once
#include "Scene/Component.h"
#include "Render/SkyLightEnvironment.h"

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

		/** Degrees; applied to IBL reflection + sky cubemap (same convention as legacy SetIBLRotate / CubeBackground). */
		void SetIBLRotationPitchDegrees(float InPitchDeg);
		void SetIBLRotationYawDegrees(float InYawDeg);
		float GetIBLRotationPitchDegrees() const { return IBLRotationPitchDegrees; }
		float GetIBLRotationYawDegrees() const { return IBLRotationYawDegrees; }

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
		float IBLRotationPitchDegrees = 0.f;
		float IBLRotationYawDegrees = 0.f;
	};
	DECLARE_COMPONENT_TRAITS_CLASS_NAME(SkyLightComponent);
}
