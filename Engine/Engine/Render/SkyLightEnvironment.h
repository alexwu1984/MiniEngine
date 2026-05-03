#pragma once
#include "core/inc.h"
#include "tinygltf/json.h"
#include <optional>
#include <string>

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

	/** Skylight environment: precompute HDR → cubemap, diffuse irradiance, specular prefilter, BRDF LUT (FSkyLightIBLPrecompute). */
	class FSkyLightIBLPrecompute
	{
	public:
		FSkyLightIBLPrecompute(RenderCore::DynamicRHI* RHI);
		~FSkyLightIBLPrecompute();

		void InitResource();
		void LoadConfig(const nlohmann::json& Root);
		void LoadTex(const std::wstring& FileName);
		/** Game thread supplies primary SkyLightComponent path each frame; falls back to JSON Evn.Hdr when nullopt. */
		void ResolveAndApplyHDRSource(std::optional<std::wstring> ComponentOverrideFullPath);
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
