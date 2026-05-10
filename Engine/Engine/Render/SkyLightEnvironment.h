#pragma once
#include "core/inc.h"
#include "tinygltf/json.h"
#include "math/vector3.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHITextureCube;
	class RHITexture2D;
}

namespace Engine
{
	struct FSkyLightEnvironmentPrecomputeState;
	struct FSkyLightEnvironmentRDGPasses;

	/** Which source the skylight environment should use this frame. */
	enum class ESkyLightSourceType : uint8_t
	{
		/** No skylight source (IBL disabled). */
		None = 0,
		/** Use a file-backed HDR lat-long (SRV Texture2D). */
		HdrFile = 1,
		/** Use procedural sky (captured directly into cubemap via shader pass). */
		Procedural = 2,
	};

	struct FSkyLightSourceDesc
	{
		ESkyLightSourceType Type = ESkyLightSourceType::None;
		/** Full path to HDR file when Type==HdrFile. */
		std::wstring HdrFileFullPath;
		/** World-space direction toward the sun/light source when Type==Procedural. */
		math::Vector3 ProceduralSunDirectionTowardSource{ 1.f, 0.05f, 0.f };
		/**
		 * Extra linear HDR added only in the fullscreen sky pass along the sun direction (does not change baked cubemap / IBL).
		 * Drives bloom threshold; set 0 to disable. Ignored when Type!=Procedural.
		 */
		float ProceduralSunBloomLinearHDR = 0.f;
	};

	/**
	 * Render-thread skylight IBL baker (UE USkyLightComponent analogue): HDR or procedural radiance to cubemap,
	 * irradiance / specular prefilter / BRDF LUT.
	 */
	class USkyLightComponent
	{
	public:
		USkyLightComponent(RenderCore::DynamicRHI* RHI);
		~USkyLightComponent();

		void InitResource();
		void LoadConfig(const nlohmann::json& Root);
		void LoadTex(const std::wstring& FileName);
		/** Render thread: selects the skylight source for this frame (keeps internal caching private). */
		void ResolveAndApplyHDRSource(const FSkyLightSourceDesc& Source);
		void Draw(RenderCore::RHICommandContext& RHIContext);
		/** Scene cut / viewport recycle: next Draw() rebuilds cubemap from current HDRTex (Resolve skips reload when HDR path unchanged). */
		void InvalidateCapturedEnvironment();
		std::shared_ptr<RenderCore::RHITextureCube> GetSkyLightCubemap();
		std::shared_ptr<RenderCore::RHITextureCube> GetDiffuseIrradianceCubemap();
		std::shared_ptr<RenderCore::RHITextureCube> GetSpecularReflectionCubemap();
		std::shared_ptr<RenderCore::RHITexture2D> GetBRDFIntegrationLUT();
		std::shared_ptr<RenderCore::RHITexture2D> GetSkyLightSourceHDR();
	private:
		friend struct FSkyLightEnvironmentRDGPasses;
		void CaptureSkyLightCubemap(RenderCore::RHICommandContext& RHIContext);
		void GenerateDiffuseIrradiance(RenderCore::RHICommandContext& RHIContext);
		void GenerateSpecularPrefilter(RenderCore::RHICommandContext& RHIContext);
		void GenerateBRDFIntegrationLUT();
	private:
		void InitShader();
		void RenderCube(RenderCore::RHICommandContext& RHIContext);
	private:
		FSkyLightEnvironmentPrecomputeState* d_ptr = nullptr;
	};
}
