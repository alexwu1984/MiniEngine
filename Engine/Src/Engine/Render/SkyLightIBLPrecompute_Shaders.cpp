#include "Render/SkyLightEnvironment.h"
#include "Render/SkyLightIBLPrecomputePrivate.h"
#include "RHI/RHIShaderDefine.h"
#include "core/system.h"
#include "core/logger.h"

using namespace RenderCore;

namespace Engine
{
	void FSkyLightIBLPrecompute::InitShader()
	{
		C_P(FSkyLightIBLPrecompute);
		std::wstring ShaderDir = core::process_directory().wstring() + L"/ShaderLibDX/";
		const std::wstring SkyIblShaderPath = ShaderDir + L"EnvironmentSkyIBL.hlsl";
		const std::wstring LongLatShaderPath = ShaderDir + L"IBLLongLatToCube.hlsl";

		RHIVertexDeclare VertexDeclareRHI;
		VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(0, EVertexElementType::VET_Float3, false));

		d->VertexShader = d->RHI->RHICreateVertexShader(SkyIblShaderPath, "VS_SkyCube", VertexDeclareRHI, {});
		d->VertexShaderLongLatToCube = d->RHI->RHICreateVertexShader(LongLatShaderPath, "VS_SkyCube", VertexDeclareRHI, {});
		d->IrrPixelShader = d->RHI->RHICreatePixelShader(SkyIblShaderPath, "PS_GenIrradiance", {});
		d->PSLongLatToCube = d->RHI->RHICreatePixelShader(LongLatShaderPath, "PS_LongLatToCube", {});
		d->PSGenPrefiltered = d->RHI->RHICreatePixelShader(SkyIblShaderPath, "PS_GenPrefiltered", {});
		if (!d->VertexShader || !d->VertexShaderLongLatToCube || !d->PSLongLatToCube || !d->IrrPixelShader || !d->PSGenPrefiltered)
		{
			core::LOG(core::log_err,
				L"FSkyLightIBLPrecompute::InitShader failed (missing shader). Check ShaderLibDX next to process_directory() and compile log.");
		}
	}

} // namespace Engine
