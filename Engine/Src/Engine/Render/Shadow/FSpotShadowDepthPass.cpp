#include "Render/Shadow/FSpotShadowDepthPass.h"
#include "Render/Shadow/FShadowDepthMeshDrawer.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIRenderTarget.h"
#include "core/color.h"
#include "Scene/SceneMeshComponent.h"
#include <cmath>

namespace Engine
{
	int FSpotShadowDepthPass::FindSpotShadowLightIndex(const std::vector<Light>& Lights)
	{
		for (int i = 0; i < static_cast<int>(Lights.size()); ++i)
		{
			const Light& L = Lights[static_cast<size_t>(i)];
			if (L.Type == LightType_Spot && L.ShadowMapIndex == kSpotLightShadowMapIndex)
				return i;
		}
		return -1;
	}

	static float MaxAlongAxisFromEyeToAabb(const math::Vector3& eye, const math::Vector3& axisUnit, const math::AABB3& box)
	{
		math::Vector3 corners[8]{};
		box.GetPoint(corners);
		float maxAlong = 0.f;
		for (int i = 0; i < 8; ++i)
		{
			const float along = math::Vector3::Dot(corners[i] - eye, axisUnit);
			if (along > maxAlong)
				maxAlong = along;
		}
		return maxAlong;
	}

	void FSpotShadowDepthPass::SetupSpotShadowViewProjection(Light& spotLight, const math::AABB3* pSceneBoundsWorld, bool bSceneBoundsValid)
	{
		math::Vector3 axis = spotLight.Direction * (-1.f);
		if (axis.GetSqrLength() < 1e-10f)
			axis = math::Vector3(0.f, -1.f, 0.f);
		axis = axis.Normalize();
		const math::Vector3 eye = spotLight.Position;
		const math::Vector3 at = eye + axis;
		math::Vector3 up = math::Vector3::UnitY;
		if (math::Abs(math::Vector3::Dot(axis, up)) > 0.98f)
			up = math::Vector3(0.f, 0.f, 1.f);
		const math::Matrix4x4 view = math::Matrix4x4::MatrixLookAtLH(eye, at, up);
		const float outerCos = math::Clamp(spotLight.OuterConeCos, -1.f, 1.f);
		float fovY = 2.f * std::acos(outerCos) + 0.12f;
		if (fovY > 3.12f)
			fovY = 3.12f;
		const float zNear = 0.05f;
		static constexpr float kSpotShadowZFarWhenRangeUnlimited = 2500.f;
		static constexpr float kSpotShadowZFarMaxClamp = 48000.f;
		float zFar = (spotLight.Range > 0.f) ? (std::max)(spotLight.Range, zNear + 0.1f) : kSpotShadowZFarWhenRangeUnlimited;
		if (bSceneBoundsValid && pSceneBoundsWorld)
		{
			const float along = MaxAlongAxisFromEyeToAabb(eye, axis, *pSceneBoundsWorld);
			const float need = along * 1.12f + 12.f;
			zFar = (std::max)(zFar, need);
		}
		zFar = (std::min)(zFar, kSpotShadowZFarMaxClamp);
		const math::Matrix4x4 proj = math::Matrix4x4::MatrixPerspectiveFovLH(fovY, 1.f, zNear, zFar);
		spotLight.LightView = view;
		spotLight.LightViewProj = view * proj;
	}

	void FSpotShadowDepthPass::Render(RenderCore::RHICommandContext& RHIContext, const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, Light& SpotLight, int SpotLightIndex,
									  const std::shared_ptr<RenderCore::RHIRenderTarget>& SpotShadowBuffer, FShadowDepthMeshDrawer& MeshDrawer, FOutputs& OutOutputs)
	{
		OutOutputs.bCachedSpotShadowValid = false;
		if (!SpotShadowBuffer)
			return;

		RHIContext.Clear(SpotShadowBuffer, core::FLinearColor::White, 1.f, 0);
		const auto TargetSize = SpotShadowBuffer->GetSize();
		RHIContext.SetViewPort(0, 0, TargetSize.x, TargetSize.y);
		MeshDrawer.DrawDirectional(RHIContext, ShadowCasterMeshes, SpotLight, SpotShadowBuffer);

		OutOutputs.CachedSpotLightViewProj = SpotLight.LightViewProj;
		OutOutputs.CachedSpotLightView = SpotLight.LightView;
		OutOutputs.CachedSpotShadowLightIndex = SpotLightIndex;
		OutOutputs.bCachedSpotShadowValid = true;
	}
}
