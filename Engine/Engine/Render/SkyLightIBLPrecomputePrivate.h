#pragma once
#include "RHI/DynamicRHI.h"
#include "Render/MaterialPreFrame.h"
#include "Render/CubeRender.h"
#include "RHI/RHITextureCube.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIShdader.h"
#include "math/matrix4x4.h"

namespace Engine
{
constexpr int kSkyLightIBL_CubeMapSize = 512;
constexpr int kSkyLightIBL_IrradianceSize = 256;
constexpr int kSkyLightIBL_PrefilterSize = 256;

inline uint32_t SkyLightIBL_ComputeNumMips(uint32_t Width, uint32_t Height)
{
	uint32_t HighBit;
	_BitScanReverse((unsigned long*)&HighBit, Width | Height);
	return HighBit + 1;
}

/** D-pointer layout for FSkyLightIBLPrecompute; kept out of SkyLightEnvironment.h so implementation can split across .cpp files. */
struct FSkyLightIBLPrecomputePrivate
{
	std::shared_ptr<RenderCore::RHITextureCube> PreFilterCube;
	std::shared_ptr<RenderCore::RHITextureCube> IrrCube;
	std::shared_ptr<RenderCore::RHITextureCube> EvnCube;
	std::shared_ptr<RenderCore::RHITexture2D> HDRTex;
	std::shared_ptr<RenderCore::RHITexture2D> PreBRDF;

	std::shared_ptr<RenderCore::RHIVertexShader> VertexShader;
	std::shared_ptr<RenderCore::RHIVertexShader> VertexShaderLongLatToCube;
	std::shared_ptr<RenderCore::RHIPixelShader> IrrPixelShader;
	std::shared_ptr<RenderCore::RHIPixelShader> PSLongLatToCube;
	std::shared_ptr<RenderCore::RHIPixelShader> PSGenPrefiltered;
	std::shared_ptr<CubeRender> CubeR;
	RenderCore::DynamicRHI* RHI = nullptr;
	std::array<math::Matrix4x4, 6> CaptureViews{};

	FSkyLightIBLPrecomputePrivate(RenderCore::DynamicRHI* InRHI)
		: GET_SHADER_STRUCT_MEMBER(ENVContant)(InRHI),
		  GET_SHADER_STRUCT_MEMBER(CBPerFrame)(InRHI),
		  GET_SHADER_STRUCT_MEMBER(CBPerObject)(InRHI),
		  RHI(InRHI)
	{
	}

	DECLARE_SHADER_STRUCT_MEMBER(ENVContant);
	DECLARE_SHADER_STRUCT_MEMBER(CBPerFrame);
	DECLARE_SHADER_STRUCT_MEMBER(CBPerObject);
	bool bInitRender = false;
	std::wstring ConfigHdrFullPath;
	std::wstring LastAppliedHdrFullPath;
	/** Serialize HDR path + HDRTex updates vs ResolveSkyLightForFrame / LoadConfig (different render-queue commands or future game-thread readers). */
	mutable std::mutex HdrStateMutex;
};

} // namespace Engine
