#include "Render/SkyLightEnvironment.h"
#include "Render/SkyLightEnvironmentPrecomputeState.h"
#include "RHI/DynamicRHI.h"
#include "core/system.h"
#include <cctype>
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
		CaptureSkyLightCubemap(RHIContext);
		GenerateDiffuseIrradiance(RHIContext);
		GenerateSpecularPrefilter(RHIContext);
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

	void USkyLightComponent::LoadConfig(const nlohmann::json& Root)
	{
		try
		{
			SKYLIGHT_IBL_DPTR();
			nlohmann::json EvnJson = Root["Evn"];
			const std::string hdrUtf8 = EvnJson["Hdr"].get<std::string>();
			std::lock_guard<std::mutex> Lock(d->Host.HdrStateMutex);
			if (JsonHdrTokenIsProceduralSky(hdrUtf8))
			{
				d->Host.ConfigSource.Type = ESkyLightSourceType::Procedural;
				d->Host.ConfigSource.HdrFileFullPath.clear();
				d->Host.ConfigSource.ProceduralSunDirectionTowardSource = math::Vector3(1.f, 0.05f, 0.f);
			}
			else
			{
				d->Host.ConfigSource.Type = ESkyLightSourceType::HdrFile;
				d->Host.ConfigSource.HdrFileFullPath =
					core::process_directory().wstring() + L"/GLTFModel/" + core::u8_ucs2(hdrUtf8);
			}
			d->Host.bInitRender = false;
			d->Host.CurrentSource = {};
		}
		catch (const std::exception&)
		{
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
			if (sameType && samePath && d->Host.bInitRender)
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
				d->SpecifiedCubemap.HDRTex = d->Bake.RHI->RHICreateHDRTexture2D(Desired.HdrFileFullPath);
			else
				d->SpecifiedCubemap.HDRTex.reset();

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
		d->Host.bInitRender = false;
	}

} // namespace Engine
