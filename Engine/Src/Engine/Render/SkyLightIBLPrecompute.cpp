#include "Render/SkyLightEnvironment.h"
#include "Render/SkyLightIBLPrecomputePrivate.h"
#include "RHI/DynamicRHI.h"

namespace Engine
{
	FSkyLightIBLPrecompute::FSkyLightIBLPrecompute(RenderCore::DynamicRHI* RHI)
		: d_ptr(new FSkyLightIBLPrecomputePrivate(RHI))
	{
	}

	FSkyLightIBLPrecompute::~FSkyLightIBLPrecompute()
	{
		delete d_ptr;
	}

	void FSkyLightIBLPrecompute::InitResource()
	{
		C_P(FSkyLightIBLPrecompute);

		InitShader();

		d->CaptureViews = {
			math::Matrix4x4::MatrixLookAtLH(math::Vector3(), math::Vector3::UnitX, math::Vector3::UnitY),
			math::Matrix4x4::MatrixLookAtLH(math::Vector3(), math::Vector3::NegUnitX, math::Vector3::UnitY),
			math::Matrix4x4::MatrixLookAtLH(math::Vector3(), math::Vector3::UnitY, math::Vector3::NegUnitZ),
			math::Matrix4x4::MatrixLookAtLH(math::Vector3(), math::Vector3::NegUnitY, math::Vector3::UnitZ),
			math::Matrix4x4::MatrixLookAtLH(math::Vector3(), math::Vector3::UnitZ, math::Vector3::UnitY),
			math::Matrix4x4::MatrixLookAtLH(math::Vector3(), math::Vector3::NegUnitZ, math::Vector3::UnitY)
		};
		d->EvnCube = d->RHI->RHICreateTextureCube(RenderCore::PF_A16B16G16R16, kSkyLightIBL_CubeMapSize, kSkyLightIBL_CubeMapSize,
			SkyLightIBL_ComputeNumMips(kSkyLightIBL_CubeMapSize, kSkyLightIBL_CubeMapSize), false);
		d->PreFilterCube = d->RHI->RHICreateTextureCube(RenderCore::PF_A16B16G16R16, kSkyLightIBL_IrradianceSize, kSkyLightIBL_IrradianceSize,
			SkyLightIBL_ComputeNumMips(kSkyLightIBL_IrradianceSize, kSkyLightIBL_IrradianceSize), false);
		d->IrrCube = d->RHI->RHICreateTextureCube(RenderCore::PF_A16B16G16R16, kSkyLightIBL_PrefilterSize, kSkyLightIBL_PrefilterSize,
			SkyLightIBL_ComputeNumMips(kSkyLightIBL_PrefilterSize, kSkyLightIBL_PrefilterSize), false);
		d->CubeR = std::make_shared<CubeRender>(d->RHI);
		d->CubeR->InitResource();
		GenerateBRDFIntegrationLUT();
	}

	void FSkyLightIBLPrecompute::Draw(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(FSkyLightIBLPrecompute);
		if (!d->HDRTex || d->bInitRender)
		{
			return;
		}
		d->bInitRender = true;
		CaptureSkyLightCubemap(RHIContext);
		GenerateDiffuseIrradiance(RHIContext);
		GenerateSpecularPrefilter(RHIContext);
	}

	std::shared_ptr<RenderCore::RHITextureCube> FSkyLightIBLPrecompute::GetSkyLightCubemap()
	{
		C_P(FSkyLightIBLPrecompute);
		return d->EvnCube;
	}

	std::shared_ptr<RenderCore::RHITextureCube> FSkyLightIBLPrecompute::GetDiffuseIrradianceCubemap()
	{
		C_P(FSkyLightIBLPrecompute);
		return d->IrrCube;
	}

	std::shared_ptr<RenderCore::RHITextureCube> FSkyLightIBLPrecompute::GetSpecularReflectionCubemap()
	{
		C_P(FSkyLightIBLPrecompute);
		return d->PreFilterCube;
	}

	std::shared_ptr<RenderCore::RHITexture2D> FSkyLightIBLPrecompute::GetBRDFIntegrationLUT()
	{
		C_P(FSkyLightIBLPrecompute);
		return d->PreBRDF;
	}

	std::shared_ptr<RenderCore::RHITexture2D> FSkyLightIBLPrecompute::GetSkyLightSourceHDR()
	{
		C_P(FSkyLightIBLPrecompute);
		return d->HDRTex;
	}

} // namespace Engine
