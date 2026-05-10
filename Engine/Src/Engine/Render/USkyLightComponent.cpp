#include "Render/SkyLightEnvironment.h"
#include "Render/SkyLightEnvironmentPrecomputeState.h"
#include "RHI/DynamicRHI.h"
#include "core/logger.h"
#include "core/system.h"
#include <chrono>
#include <cctype>
#include <cmath>
#include <string>

namespace Engine
{
	namespace
	{
		static bool JsonHdrTokenIsProceduralSky(const std::string& utf8)
		{
			std::string lower;
			lower.reserve(utf8.size());
			for (unsigned char c : utf8)
			{
				if (c == ' ')
					lower.push_back('_');
				else
					lower.push_back(char(std::tolower(c)));
			}
			return lower == "proceduralsky" || lower == "procedural_sky";
		}
	} // namespace

	USkyLightComponent::USkyLightComponent(RenderCore::DynamicRHI* RHI)
		: d_ptr(new FSkyLightEnvironmentPrecomputeState(RHI))
	{
	}

	USkyLightComponent::~USkyLightComponent()
	{
		delete d_ptr;
	}

	void USkyLightComponent::InitShader()
	{
		SKYLIGHT_IBL_DPTR();
		const std::wstring ShaderDir = core::process_directory().wstring() + L"/ShaderLibDX/";
		d->Bake.InitSharedShaders(ShaderDir);
		d->ProceduralSky.InitCubemapPixelShader(d->Bake.RHI, ShaderDir);
	}

	void USkyLightComponent::InitResource()
	{
		SKYLIGHT_IBL_DPTR();

		InitShader();

		d->Bake.InitTexturesAndCubeRender();
		d->Bake.GenerateBRDFIntegrationLUT();
	}

	void USkyLightComponent::CaptureSkyLightCubemap(RenderCore::RHICommandContext& RHIContext)
	{
		SKYLIGHT_IBL_DPTR();
		if (d->Host.bProceduralSkyActive)
			d->ProceduralSky.CaptureRadianceCubemap(RHIContext, d->Bake);
		else
			d->SpecifiedCubemap.CaptureRadianceCubemap(RHIContext, d->Bake);
	}

	void USkyLightComponent::GenerateDiffuseIrradiance(RenderCore::RHICommandContext& RHIContext)
	{
		SKYLIGHT_IBL_DPTR();
		d->Bake.GenerateDiffuseIrradiance(RHIContext);
	}

	void USkyLightComponent::GenerateSpecularPrefilter(RenderCore::RHICommandContext& RHIContext)
	{
		SKYLIGHT_IBL_DPTR();
		d->Bake.GenerateSpecularPrefilter(RHIContext);
	}

	void USkyLightComponent::GenerateBRDFIntegrationLUT()
	{
		SKYLIGHT_IBL_DPTR();
		d->Bake.GenerateBRDFIntegrationLUT();
	}

	void USkyLightComponent::RenderCube(RenderCore::RHICommandContext& RHIContext)
	{
		SKYLIGHT_IBL_DPTR();
		d->Bake.RenderCube(RHIContext);
	}

	void USkyLightComponent::Draw(RenderCore::RHICommandContext& RHIContext)
	{
		SKYLIGHT_IBL_DPTR();
		{
			std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
			if (d->Host.bInitRender)
				return;
			if (!d->Host.bProceduralSkyActive && !d->SpecifiedCubemap.HDRTex)
				return;
		}

		{
			std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
			if (d->Host.bInitRender)
				return;
			if (!d->Host.bProceduralSkyActive && !d->SpecifiedCubemap.HDRTex)
				return;
			d->Host.bInitRender = true;
		}
		const auto iblBakeStart = std::chrono::steady_clock::now();
		CaptureSkyLightCubemap(RHIContext);
		GenerateDiffuseIrradiance(RHIContext);
		GenerateSpecularPrefilter(RHIContext);
		const double iblMs =
			std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - iblBakeStart).count();
		core::inf() << "IBL: Draw full bake (capture + irradiance + prefilter, wall CPU wait incl. GPU submit) " << iblMs << " ms\n";
	}

	void USkyLightComponent::InvalidateCapturedEnvironment()
	{
		SKYLIGHT_IBL_DPTR();
		std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
		d->Host.bInitRender = false;
	}

	std::shared_ptr<RenderCore::RHITextureCube> USkyLightComponent::GetSkyLightCubemap()
	{
		SKYLIGHT_IBL_DPTR();
		std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
		return d->Bake.EvnCube;
	}

	std::shared_ptr<RenderCore::RHITextureCube> USkyLightComponent::GetDiffuseIrradianceCubemap()
	{
		SKYLIGHT_IBL_DPTR();
		std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
		return d->Bake.IrrCube;
	}

	std::shared_ptr<RenderCore::RHITextureCube> USkyLightComponent::GetSpecularReflectionCubemap()
	{
		SKYLIGHT_IBL_DPTR();
		std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
		return d->Bake.PreFilterCube;
	}

	std::shared_ptr<RenderCore::RHITexture2D> USkyLightComponent::GetBRDFIntegrationLUT()
	{
		SKYLIGHT_IBL_DPTR();
		std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
		return d->Bake.PreBRDF;
	}

	std::shared_ptr<RenderCore::RHITexture2D> USkyLightComponent::GetSkyLightSourceHDR()
	{
		SKYLIGHT_IBL_DPTR();
		std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
		return d->SpecifiedCubemap.HDRTex;
	}

	std::shared_ptr<RenderCore::RHITexture2D> USkyLightComponent::GetGroundHemiIBLLatLong()
	{
		SKYLIGHT_IBL_DPTR();
		std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
		return d->Host.GroundHemiLatLongTex;
	}

	bool USkyLightComponent::HasSplitHemisphereGroundIBL()
	{
		SKYLIGHT_IBL_DPTR();
		std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
		return d->Host.bProceduralSkyActive && d->Host.GroundHemiLatLongTex != nullptr;
	}

	float USkyLightComponent::GetGroundIBLIntensityForShader()
	{
		SKYLIGHT_IBL_DPTR();
		std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
		return d->Host.CurrentGroundIBLIntensity;
	}

	float USkyLightComponent::GetHemiIBLBlendPowerForShader()
	{
		SKYLIGHT_IBL_DPTR();
		std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
		return d->Host.CurrentHemiIBLBlendPower;
	}

	void USkyLightComponent::LoadConfig(const nlohmann::json& Root)
	{
		SKYLIGHT_IBL_DPTR();
		try
		{
			nlohmann::json EvnJson = Root["Evn"];
			const std::string hdrUtf8 = EvnJson["Hdr"].get<std::string>();
			std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
			if (JsonHdrTokenIsProceduralSky(hdrUtf8))
			{
				d->Host.ConfigSource.Type = ESkyLightSourceType::Procedural;
				d->Host.ConfigSource.HdrFileFullPath.clear();
				d->Host.ConfigSource.ProceduralSunDirectionTowardSource = math::Vector3(1.f, 0.05f, 0.f);
				d->Host.ConfigGroundIBLHdrUtf8.clear();
				d->Host.ConfigGroundIBLIntensity = EvnJson.value("GroundIBLIntensity", 1.0);
				d->Host.ConfigHemiIBLBlendPower = EvnJson.value("HemiIBLBlendPower", 1.75);
				auto groundIt = EvnJson.find("GroundIBLHdr");
				if (groundIt != EvnJson.end() && groundIt->is_string())
				{
					std::string groundHdr = groundIt->get<std::string>();
					if (!groundHdr.empty())
						d->Host.ConfigGroundIBLHdrUtf8 = std::move(groundHdr);
				}
			}
			else
			{
				d->Host.ConfigSource.Type = ESkyLightSourceType::HdrFile;
				d->Host.ConfigSource.HdrFileFullPath =
					core::process_directory().wstring() + L"/GLTFModel/" + core::u8_ucs2(hdrUtf8);
				d->Host.ConfigGroundIBLHdrUtf8.clear();
				d->Host.ConfigGroundIBLIntensity = 1.f;
				d->Host.ConfigHemiIBLBlendPower = 1.75f;
			}
			d->Host.bInitRender = false;
			d->Host.CurrentSource = {};
		}
		catch (const std::exception&)
		{
			std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
			d->Host.bInitRender = false;
			d->Host.CurrentSource = {};
			d->Host.ConfigGroundIBLHdrUtf8.clear();
		}
	}

	void USkyLightComponent::ResolveAndApplyHDRSource(const FSkyLightSourceDesc& Source)
	{
		SKYLIGHT_IBL_DPTR();
		FSkyLightSourceDesc Desired{};
		{
			std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
			Desired = Source;
			if (Desired.Type == ESkyLightSourceType::None)
				Desired = d->Host.ConfigSource;
		}

		{
			std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
			const bool sameType = (Desired.Type == d->Host.CurrentSource.Type);
			const bool samePath = (Desired.HdrFileFullPath == d->Host.CurrentSource.HdrFileFullPath);
			bool proceduralSunMatches = true;
			if (Desired.Type == ESkyLightSourceType::Procedural && d->Host.CurrentSource.Type == ESkyLightSourceType::Procedural)
			{
				const math::Vector3 dv = Desired.ProceduralSunDirectionTowardSource - d->Host.CurrentSource.ProceduralSunDirectionTowardSource;
				proceduralSunMatches = dv.Dot(dv) < 1e-8f;
			}
			const bool sameGroundHdr = (d->Host.ConfigGroundIBLHdrUtf8 == d->Host.CurrentGroundIBLHdrUtf8);
			const bool sameGroundInt =
				std::fabs(d->Host.ConfigGroundIBLIntensity - d->Host.CurrentGroundIBLIntensity) < 1e-5f;
			const bool sameHemiPow =
				std::fabs(d->Host.ConfigHemiIBLBlendPower - d->Host.CurrentHemiIBLBlendPower) < 1e-5f;
			if (sameType && samePath && d->Host.bInitRender && proceduralSunMatches && sameGroundHdr && sameGroundInt && sameHemiPow)
				return;

			d->Host.CurrentSource = Desired;
			d->Host.bProceduralSkyActive = (Desired.Type == ESkyLightSourceType::Procedural);
			if (d->Host.bProceduralSkyActive)
			{
				math::Vector3 dir = Desired.ProceduralSunDirectionTowardSource;
				if (dir.GetSqrLength() < 1e-10f)
					dir = math::Vector3(1.f, 0.05f, 0.f);
				dir = dir.Normalize();
				d->ProceduralSky.ProceduralSunDirX = dir.x;
				d->ProceduralSky.ProceduralSunDirY = dir.y;
				d->ProceduralSky.ProceduralSunDirZ = dir.z;
			}

			if (Desired.Type == ESkyLightSourceType::HdrFile && !Desired.HdrFileFullPath.empty())
			{
				const auto tHdr = std::chrono::steady_clock::now();
				d->SpecifiedCubemap.HDRTex = d->Bake.RHI->RHICreateHDRTexture2D(Desired.HdrFileFullPath);
				const double hdrMs =
					std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tHdr).count();
				core::inf() << "IBL: RHICreateHDRTexture2D (sky latlong) " << hdrMs << " ms " << Desired.HdrFileFullPath << "\n";
			}
			else
				d->SpecifiedCubemap.HDRTex.reset();

			if (d->Host.bProceduralSkyActive && !d->Host.ConfigGroundIBLHdrUtf8.empty())
			{
				const std::wstring gfp =
					core::process_directory().wstring() + L"/GLTFModel/" + core::u8_ucs2(d->Host.ConfigGroundIBLHdrUtf8);
				const auto tGr = std::chrono::steady_clock::now();
				d->Host.GroundHemiLatLongTex = d->Bake.RHI->RHICreateHDRTexture2D(gfp);
				const double grMs =
					std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tGr).count();
				core::inf() << "IBL: RHICreateHDRTexture2D (ground hemi latlong) " << grMs << " ms " << gfp << "\n";
			}
			else
				d->Host.GroundHemiLatLongTex.reset();

			d->Host.CurrentGroundIBLHdrUtf8 = d->Host.ConfigGroundIBLHdrUtf8;
			d->Host.CurrentGroundIBLIntensity = d->Host.ConfigGroundIBLIntensity;
			d->Host.CurrentHemiIBLBlendPower = d->Host.ConfigHemiIBLBlendPower;

			d->Host.bInitRender = false;
		}
	}

	void USkyLightComponent::LoadTex(const std::wstring& FileName)
	{
		SKYLIGHT_IBL_DPTR();
		std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
		d->Host.ConfigSource.Type = ESkyLightSourceType::HdrFile;
		d->Host.ConfigSource.HdrFileFullPath = FileName;
		d->Host.CurrentSource = {};
		d->SpecifiedCubemap.HDRTex.reset();
		d->Host.bProceduralSkyActive = false;
		d->Host.GroundHemiLatLongTex.reset();
		d->Host.CurrentGroundIBLHdrUtf8.clear();
		d->Host.bInitRender = false;
	}

} // namespace Engine
