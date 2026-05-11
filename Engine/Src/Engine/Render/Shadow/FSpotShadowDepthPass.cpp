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

	void FSpotShadowDepthPass::Render(const FSpotShadowDepthPassParameters& P)
	{
		if (!P.OutOutputs || !P.RHICmdList || !P.FrameLights || !P.MeshDrawer || !P.SpotShadowBuffer)
			return;

		FOutputs& OutOutputs = *P.OutOutputs;
		OutOutputs.bCachedSpotShadowValid = false;

		const std::vector<GltfSceneMeshInfo>* meshList = nullptr;
		if (P.ShadowCasterMeshes && !P.ShadowCasterMeshes->empty())
			meshList = P.ShadowCasterMeshes;
		else if (P.FrustumBoundsMeshes && !P.FrustumBoundsMeshes->empty())
			meshList = P.FrustumBoundsMeshes;

		if (!meshList || P.SpotLightListIndex < 0)
			return;

		Light& spotL = (*P.FrameLights)[static_cast<size_t>(P.SpotLightListIndex)];
		math::AABB3 spotZFarBounds{};
		bool spotZFarOk = false;
		if (P.bSubjectValid)
		{
			spotZFarBounds = P.SubjectWorldAabb;
			spotZFarOk = true;
		}
		if (P.bReceiverValid)
		{
			spotZFarBounds = spotZFarOk ? spotZFarBounds.MergeAABB(P.ReceiverWorldAabb) : P.ReceiverWorldAabb;
			spotZFarOk = true;
		}
		SetupSpotShadowViewProjection(spotL, spotZFarOk ? &spotZFarBounds : nullptr, spotZFarOk);

		RenderCore::RHICommandContext& RHIContext = *P.RHICmdList;
		FShadowDepthMeshDrawer& MeshDrawer = *P.MeshDrawer;
		const std::shared_ptr<RenderCore::RHIRenderTarget>& SpotShadowBuffer = P.SpotShadowBuffer;

		RHIContext.Clear(SpotShadowBuffer, core::FLinearColor::White, 1.f, 0);
		const auto TargetSize = SpotShadowBuffer->GetSize();
		RHIContext.SetViewPort(0, 0, TargetSize.x, TargetSize.y);
		MeshDrawer.DrawDirectional(RHIContext, *meshList, spotL, SpotShadowBuffer);

		OutOutputs.CachedSpotLightViewProj = spotL.LightViewProj;
		OutOutputs.CachedSpotLightView = spotL.LightView;
		OutOutputs.CachedSpotShadowLightIndex = P.SpotLightListIndex;
		OutOutputs.bCachedSpotShadowValid = true;
	}
}
