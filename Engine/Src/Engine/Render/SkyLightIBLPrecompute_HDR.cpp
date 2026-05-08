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

		static bool TryParseFirstDirectionalLightDir(const nlohmann::json& EvnJson, float& sx, float& sy, float& sz)
		{
			try
			{
				const auto& arr = EvnJson.at("Light");
				if (!arr.is_array())
					return false;
				for (const auto& lj : arr)
				{
					if (!lj.is_object() || lj.find("LightType") == lj.end() || !lj.at("LightType").is_number_integer())
						continue;
					if (lj.at("LightType").get<int>() != LightType_Directional)
						continue;
					const std::string dirStr = lj.at("LightDir").get<std::string>();
					float x = 0.f, y = 0.f, z = 0.f;
					if (std::sscanf(dirStr.c_str(), "%f,%f,%f", &x, &y, &z) != 3)
						return false;
					sx = x;
					sy = y;
					sz = z;
					return true;
				}
			}
			catch (const std::exception&)
			{
			}
			return false;
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
				float sx = 0.f, sy = 0.49f, sz = 0.833f;
				float px = sx, py = sy, pz = sz;
				if (TryParseFirstDirectionalLightDir(EvnJson, px, py, pz))
				{
					sx = px;
					sy = py;
					sz = pz;
				}
				d->ProceduralSunDirX = sx;
				d->ProceduralSunDirY = sy;
				d->ProceduralSunDirZ = sz;
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
