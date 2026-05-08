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
	class FRDGBuilder;
	struct FSkyLightIBLPrecomputePrivate;

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
		math::Vector3 ProceduralSunDirectionTowardSource{ 0.f, 0.49f, 0.833f };
	};

	/** Skylight environment: precompute HDR → cubemap, diffuse irradiance, specular prefilter, BRDF LUT (FSkyLightIBLPrecompute). */
	class FSkyLightIBLPrecompute
	{
	public:
		FSkyLightIBLPrecompute(RenderCore::DynamicRHI* RHI);
		~FSkyLightIBLPrecompute();

		void InitResource();
		void LoadConfig(const nlohmann::json& Root);
		void LoadTex(const std::wstring& FileName);
		/** Render thread: selects the skylight source for this frame (keeps internal caching private). */
		void ResolveAndApplyHDRSource(const FSkyLightSourceDesc& Source);
		void Draw(RenderCore::RHICommandContext& RHIContext);
		/** Scene cut / viewport recycle: next Draw() rebuilds cubemap from current HDRTex (Resolve skips reload when HDR path unchanged). */
		void InvalidateCapturedEnvironment();
		void AddFramePasses(FRDGBuilder& Graph, RenderCore::RHICommandContext& RHIContext);
		std::shared_ptr<RenderCore::RHITextureCube> GetSkyLightCubemap();
		std::shared_ptr<RenderCore::RHITextureCube> GetDiffuseIrradianceCubemap();
		std::shared_ptr<RenderCore::RHITextureCube> GetSpecularReflectionCubemap();
		std::shared_ptr<RenderCore::RHITexture2D> GetBRDFIntegrationLUT();
		std::shared_ptr<RenderCore::RHITexture2D> GetSkyLightSourceHDR();
	private:
		void CaptureSkyLightCubemap(RenderCore::RHICommandContext& RHIContext);
		void GenerateDiffuseIrradiance(RenderCore::RHICommandContext& RHIContext);
		void GenerateSpecularPrefilter(RenderCore::RHICommandContext& RHIContext);
		void GenerateBRDFIntegrationLUT();
	private:
		void InitShader();
		void RenderCube(RenderCore::RHICommandContext& RHIContext);
	private:
		FSkyLightIBLPrecomputePrivate* d_ptr = nullptr;
	};
}
