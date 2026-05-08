#include "Render/SkyLightEnvironment.h"
#include "Render/SkyLightIBLPrecomputePrivate.h"
#include "core/system.h"
#include "RHI/RHIDefinitions.h"
#include <DirectXTex.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

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

	void FSkyLightIBLPrecompute::LoadConfig(const nlohmann::json& Root)
	{
		try
		{
			C_P(FSkyLightIBLPrecompute);
			nlohmann::json EvnJson = Root["Evn"];
			const std::string hdrUtf8 = EvnJson["Hdr"].get<std::string>();
			std::lock_guard<std::mutex> Lock(d->HdrStateMutex);
			if (JsonHdrTokenIsProceduralSky(hdrUtf8))
			{
				d->ConfigSource.Type = ESkyLightSourceType::Procedural;
				d->ConfigSource.HdrFileFullPath.clear();
				d->ConfigSource.ProceduralSunDirectionTowardSource = math::Vector3(0.f, 0.49f, 0.833f);
			}
			else
			{
				d->ConfigSource.Type = ESkyLightSourceType::HdrFile;
				d->ConfigSource.HdrFileFullPath = core::process_directory().wstring() + L"/GLTFModel/" + core::u8_ucs2(hdrUtf8);
			}
			d->bInitRender = false;
			d->CurrentSource = {}; // force Resolve() to apply config next frame
		}
		catch (const std::exception&)
		{
		}
	}

	void FSkyLightIBLPrecompute::ResolveAndApplyHDRSource(const FSkyLightSourceDesc& Source)
	{
		C_P(FSkyLightIBLPrecompute);
		FSkyLightSourceDesc Desired{};
		{
			std::lock_guard<std::mutex> Lock(d->HdrStateMutex);
			Desired = Source;
			// None means "use config fallback" (keeps external call-site simple).
			if (Desired.Type == ESkyLightSourceType::None)
				Desired = d->ConfigSource;
		}

		{
			std::lock_guard<std::mutex> Lock(d->HdrStateMutex);
			const bool sameType = (Desired.Type == d->CurrentSource.Type);
			const bool samePath = (Desired.HdrFileFullPath == d->CurrentSource.HdrFileFullPath);
			if (sameType && samePath && d->bInitRender)
				return;

			d->CurrentSource = Desired;
			d->bProceduralSkyActive = (Desired.Type == ESkyLightSourceType::Procedural);
			if (d->bProceduralSkyActive)
			{
				math::Vector3 dir = Desired.ProceduralSunDirectionTowardSource;
				if (dir.GetSqrLength() < 1e-10f)
					dir = math::Vector3(0.f, 0.49f, 0.833f);
				dir = dir.Normalize();
				d->ProceduralSunDirX = dir.x;
				d->ProceduralSunDirY = dir.y;
				d->ProceduralSunDirZ = dir.z;
			}

			if (Desired.Type == ESkyLightSourceType::HdrFile && !Desired.HdrFileFullPath.empty())
				d->HDRTex = d->RHI->RHICreateHDRTexture2D(Desired.HdrFileFullPath);
			else
				d->HDRTex.reset();

			d->bInitRender = false;
		}
	}

	void FSkyLightIBLPrecompute::LoadTex(const std::wstring& FileName)
	{
		C_P(FSkyLightIBLPrecompute);
		std::lock_guard<std::mutex> Lock(d->HdrStateMutex);
		d->ConfigSource.Type = ESkyLightSourceType::HdrFile;
		d->ConfigSource.HdrFileFullPath = FileName;
		d->CurrentSource = {};
		d->HDRTex.reset();
		d->bProceduralSkyActive = false;
		d->bInitRender = false;
	}

} // namespace Engine
