#include "Render/Shadow/FPointShadowCubePass.h"
#include "Render/Shadow/FShadowDepthMeshDrawer.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHITextureCube.h"
#include "core/color.h"
#include "core/vec2.h"
#include "math/math.h"
#include "Scene/SceneMeshComponent.h"

namespace Engine
{
	int FPointShadowCubePass::FindPointShadowCubeLightIndex(const std::vector<Light>& Lights)
	{
		for (int i = 0; i < static_cast<int>(Lights.size()); ++i)
		{
			const Light& L = Lights[static_cast<size_t>(i)];
			if (L.Type == LightType_Point && L.ShadowMapIndex == kPointLightCubeShadowMapIndex)
				return i;
		}
		return -1;
	}

	static math::Matrix4x4 ComputePointShadowFaceViewProj(const math::Vector3& lightPos, int face, float zNear, float zFar)
	{
		math::Vector3 forward;
		math::Vector3 up;
		switch (face)
		{
		case 0: forward = { 1.f, 0.f, 0.f }; up = { 0.f, 1.f, 0.f }; break;
		case 1: forward = { -1.f, 0.f, 0.f }; up = { 0.f, 1.f, 0.f }; break;
		case 2: forward = { 0.f, 1.f, 0.f }; up = { 0.f, 0.f, -1.f }; break;
		case 3: forward = { 0.f, -1.f, 0.f }; up = { 0.f, 0.f, 1.f }; break;
		case 4: forward = { 0.f, 0.f, 1.f }; up = { 0.f, 1.f, 0.f }; break;
		default: forward = { 0.f, 0.f, -1.f }; up = { 0.f, 1.f, 0.f }; break;
		}
		const math::Vector3 target = lightPos + forward;
		const math::Matrix4x4 view = math::Matrix4x4::MatrixLookAtLH(lightPos, target, up);
		const math::Matrix4x4 proj = math::Matrix4x4::MatrixPerspectiveFovLH(math::HALF_PI, 1.f, zNear, zFar);
		return view * proj;
	}

	void FPointShadowCubePass::Render(const FPointShadowCubePassParameters& P)
	{
		if (!P.OutOutputs || !P.RHICmdList || !P.MeshDrawer || !P.PointLight || !P.ShadowCasterMeshes || P.ShadowCasterMeshes->empty() || !P.PointShadowCube)
			return;

		FOutputs& OutOutputs = *P.OutOutputs;
		OutOutputs.bCachedPointShadowValid = false;

		RenderCore::RHICommandContext& RHIContext = *P.RHICmdList;
		const Light& PointLight = *P.PointLight;
		const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes = *P.ShadowCasterMeshes;
		FShadowDepthMeshDrawer& MeshDrawer = *P.MeshDrawer;
		const std::shared_ptr<RenderCore::RHITextureCube>& PointShadowCube = P.PointShadowCube;

		const float zNear = 0.05f;
		const float zFar = (std::max)(PointLight.Range, zNear + 0.1f);
		for (int face = 0; face < 6; ++face)
			OutOutputs.CachedPointFaceVP[face] = ComputePointShadowFaceViewProj(PointLight.Position, face, zNear, zFar);
		OutOutputs.CachedPointLightPos = PointLight.Position;
		OutOutputs.CachedPointLightRange = PointLight.Range;
		OutOutputs.CachedPointShadowLightIndex = P.PointLightListIndex;

		const core::vec2i cubeSize = PointShadowCube->GetSize();
		for (int face = 0; face < 6; ++face)
		{
			RHIContext.Clear(PointShadowCube, face, 0, core::FLinearColor::White, 1.f, 0);
			RHIContext.SetViewPort(0, 0, cubeSize.x, cubeSize.y);
			Light faceLight = PointLight;
			faceLight.LightViewProj = OutOutputs.CachedPointFaceVP[face];
			MeshDrawer.DrawCubeFace(RHIContext, ShadowCasterMeshes, faceLight, PointShadowCube, face);
		}
		OutOutputs.bCachedPointShadowValid = true;
	}
}
