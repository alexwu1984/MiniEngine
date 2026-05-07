#include "Render/SkyLightEnvironment.h"
#include "Render/SkyLightIBLPrecomputePrivate.h"
#include "core/system.h"
#include "RHI/RHIDefinitions.h"
#include <DirectXTex.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace Engine
{
	namespace
	{
		static constexpr wchar_t kProcSkySentinel[] = L"\x01SKY_PROC_IBL";

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

		static std::shared_ptr<RenderCore::RHITexture2D> CreateProceduralSkyLatLong(RenderCore::DynamicRHI* RHI)
		{
			if (!RHI)
				return {};
			constexpr int W = 2048;
			constexpr int H = 1024;
			const DXGI_FORMAT fmt = DXGI_FORMAT_R32G32B32A32_FLOAT;
			size_t rowPitch = 0;
			size_t slicePitch = 0;
			DirectX::ComputePitch(fmt, W, H, rowPitch, slicePitch);
			std::vector<uint8_t> staging(slicePitch, 0);

			const float pi = 3.14159265358979323846f;
			// Match AMD glTFSample VK Renderer procedural skydome constants (SkyDomeProc::Constants).
			// vSunDirection = (1, 0.05, 0); turbidity-ish horizon fade kept mild so the disk reads clearly after cubemap filter.
			float sx = 1.0f, sy = 0.05f, sz = 0.0f;
			const float invLen = 1.f / std::sqrt(sx * sx + sy * sy + sz * sz);
			sx *= invLen;
			sy *= invLen;
			sz *= invLen;

			for (int y = 0; y < H; ++y)
			{
				float* row = reinterpret_cast<float*>(staging.data() + static_cast<size_t>(y) * rowPitch);
				for (int x = 0; x < W; ++x)
				{
					const float lon = (static_cast<float>(x) + 0.5f) / static_cast<float>(W) * (2.f * pi);
					const float lat = (static_cast<float>(y) + 0.5f) / static_cast<float>(H) * pi;
					const float sinLat = std::sin(lat);
					const float dx = sinLat * std::sin(lon);
					const float dy = std::cos(lat);
					const float dz = sinLat * std::cos(lon);

					const float elev = std::max(dy, 0.f);
					const float horizonBlend = std::pow(elev, 0.48f);
					float r = 0.76f + (0.22f - 0.76f) * horizonBlend;
					float g = 0.82f + (0.48f - 0.82f) * horizonBlend;
					float b = 0.92f + (0.95f - 0.92f) * horizonBlend;

					const float sunDot = dx * sx + dy * sy + dz * sz;
					// Tight core + wide corona (pre-filter still leaves a visible sun for bloom / specular).
					const float sunCore = std::pow(std::max(sunDot, 0.f), 2048.f) * 85.f;
					const float sunGlow = std::pow(std::max(sunDot, 0.f), 96.f) * 14.f;
					const float sunHalo = std::pow(std::max(sunDot, 0.f), 28.f) * 2.8f;
					const float sun = sunCore + sunGlow + sunHalo;
					r += sun * 1.05f;
					g += sun * 0.98f;
					b += sun * 0.85f;

					row[x * 4 + 0] = r;
					row[x * 4 + 1] = g;
					row[x * 4 + 2] = b;
					row[x * 4 + 3] = 1.f;
				}
			}

			return RHI->RHICreateTexture2D(RenderCore::PF_A32B32G32R32F, RenderCore::TexCreate_ShaderResource, W, H, 1,
										   staging.data(), 0);
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
				d->bConfigProceduralSky = true;
				d->ConfigHdrFullPath.clear();
				d->HDRTex = CreateProceduralSkyLatLong(d->RHI);
				d->LastAppliedHdrFullPath = kProcSkySentinel;
			}
			else
			{
				d->bConfigProceduralSky = false;
				std::wstring HdrFile = core::process_directory().wstring() + L"/GLTFModel/" + core::u8_ucs2(hdrUtf8);
				d->ConfigHdrFullPath = HdrFile;
				d->HDRTex = d->RHI->RHICreateHDRTexture2D(HdrFile);
				d->LastAppliedHdrFullPath = HdrFile;
			}
			d->bInitRender = false;
		}
		catch (const std::exception&)
		{
		}
	}

	void FSkyLightIBLPrecompute::ResolveAndApplyHDRSource(std::optional<std::wstring> ComponentOverrideFullPath,
														 bool bSkyLightComponentProcedural)
	{
		C_P(FSkyLightIBLPrecompute);

		bool wantProc = false;
		std::wstring filePath;

		{
			std::lock_guard<std::mutex> Lock(d->HdrStateMutex);
			if (bSkyLightComponentProcedural)
				wantProc = true;
			else if (ComponentOverrideFullPath && !ComponentOverrideFullPath->empty())
				filePath = *ComponentOverrideFullPath;
			else if (d->bConfigProceduralSky)
				wantProc = true;
			else
				filePath = d->ConfigHdrFullPath;
		}

		if (wantProc)
		{
			std::lock_guard<std::mutex> Lock(d->HdrStateMutex);
			if (d->LastAppliedHdrFullPath == kProcSkySentinel && d->HDRTex)
				return;

			d->HDRTex = CreateProceduralSkyLatLong(d->RHI);
			d->LastAppliedHdrFullPath = kProcSkySentinel;
			d->bInitRender = false;
			return;
		}

		if (filePath.empty())
			return;

		{
			std::lock_guard<std::mutex> Lock(d->HdrStateMutex);
			if (filePath == d->LastAppliedHdrFullPath && d->HDRTex)
				return;

			d->HDRTex = d->RHI->RHICreateHDRTexture2D(filePath);
			d->bInitRender = false;
			d->LastAppliedHdrFullPath = std::move(filePath);
		}
	}

	void FSkyLightIBLPrecompute::LoadTex(const std::wstring& FileName)
	{
		C_P(FSkyLightIBLPrecompute);
		std::lock_guard<std::mutex> Lock(d->HdrStateMutex);
		d->HDRTex = d->RHI->RHICreateHDRTexture2D(FileName);
		d->LastAppliedHdrFullPath = FileName;
		d->bConfigProceduralSky = false;
		d->bInitRender = false;
	}

} // namespace Engine
