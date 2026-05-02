#include "Render/SkyLightEnvironment.h"
#include "Render/SkyLightIBLPrecomputePrivate.h"
#include "core/system.h"

namespace Engine
{
	void FSkyLightIBLPrecompute::LoadConfig(const nlohmann::json& Root)
	{
		try
		{
			C_P(FSkyLightIBLPrecompute);
			nlohmann::json EvnJson = Root["Evn"];
			std::wstring HdrFile = core::process_directory().wstring() + L"/GLTFModel/" + core::u8_ucs2(EvnJson["Hdr"]);
			std::lock_guard<std::mutex> Lock(d->HdrStateMutex);
			d->HDRTex = d->RHI->RHICreateHDRTexture2D(HdrFile);
			d->ConfigHdrFullPath = HdrFile;
			d->LastAppliedHdrFullPath = HdrFile;
			d->bInitRender = false;
		}
		catch (const std::exception&)
		{
		}
	}

	void FSkyLightIBLPrecompute::ResolveAndApplyHDRSource(std::optional<std::wstring> ComponentOverrideFullPath)
	{
		C_P(FSkyLightIBLPrecompute);
		std::wstring desired;
		if (ComponentOverrideFullPath && !ComponentOverrideFullPath->empty())
			desired = std::move(*ComponentOverrideFullPath);
		else
		{
			std::lock_guard<std::mutex> Lock(d->HdrStateMutex);
			desired = d->ConfigHdrFullPath;
		}

		if (desired.empty())
			return;

		{
			std::lock_guard<std::mutex> Lock(d->HdrStateMutex);
			if (desired == d->LastAppliedHdrFullPath && d->HDRTex)
				return;

			d->HDRTex = d->RHI->RHICreateHDRTexture2D(desired);
			d->bInitRender = false;
			d->LastAppliedHdrFullPath = std::move(desired);
		}
	}

	void FSkyLightIBLPrecompute::LoadTex(const std::wstring& FileName)
	{
		C_P(FSkyLightIBLPrecompute);
		std::lock_guard<std::mutex> Lock(d->HdrStateMutex);
		d->HDRTex = d->RHI->RHICreateHDRTexture2D(FileName);
		d->bInitRender = false;
	}

} // namespace Engine
