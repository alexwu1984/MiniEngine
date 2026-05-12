#include "Render/Shadow/FDirectionalShadowFrustumFitter.h"
#include "Render/Shadow/FShadowSceneBounds.h"
#include "math/aabb3.h"

namespace
{
	static constexpr float kZMargin = 4.0f;
	static constexpr float kXYMargin = 0.14f;
	static constexpr int kFitSnapIterations = 2;

	static void FitOrthoFromLightSpaceMinMax(const math::Vector3& lsMin, const math::Vector3& lsMax, float& nearValue, float& farValue, float& centerX, float& centerY,
											 float& sizeX, float& sizeY)
	{
		float zLo = lsMin.z;
		float zHi = lsMax.z;
		if (zLo > zHi)
		{
			const float t = zLo;
			zLo = zHi;
			zHi = t;
		}
		nearValue = zLo - kZMargin;
		farValue = zHi + kZMargin;
		if (nearValue >= farValue)
			farValue = nearValue + 1.0f;
		sizeX = (lsMax.x - lsMin.x) * 0.5f * (1.0f + kXYMargin);
		sizeY = (lsMax.y - lsMin.y) * 0.5f * (1.0f + kXYMargin);
		centerX = (lsMin.x + lsMax.x) * 0.5f;
		centerY = (lsMin.y + lsMax.y) * 0.5f;
	}

	static void FitOrthoFromWorldCorners(const math::Vector3 wsSceneCorners[8], const math::Matrix4x4& lightView, float& nearValue, float& farValue, float& centerX,
										 float& centerY, float& sizeX, float& sizeY)
	{
		math::Vector3 lsMin(FLT_MAX, FLT_MAX, FLT_MAX);
		math::Vector3 lsMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (int i = 0; i < 8; ++i)
		{
			math::Vector3 lsCorner = lightView.TransformPosition(wsSceneCorners[i]);
			lsMin = math::Vector3((std::min)(lsMin.x, lsCorner.x), (std::min)(lsMin.y, lsCorner.y), (std::min)(lsMin.z, lsCorner.z));
			lsMax = math::Vector3((std::max)(lsMax.x, lsCorner.x), (std::max)(lsMax.y, lsCorner.y), (std::max)(lsMax.z, lsCorner.z));
		}
		FitOrthoFromLightSpaceMinMax(lsMin, lsMax, nearValue, farValue, centerX, centerY, sizeX, sizeY);
	}

	static void ExpandOrthoDepthForReceiverBounds(const math::Matrix4x4& lightView, const math::AABB3& receiverWorldAabb, float& nearValue, float& farValue)
	{
		math::Vector3 corners[8];
		receiverWorldAabb.GetPoint(corners);
		float zMin = FLT_MAX;
		float zMax = -FLT_MAX;
		for (int i = 0; i < 8; ++i)
		{
			const math::Vector3 ls = lightView.TransformPosition(corners[i]);
			zMin = (std::min)(zMin, ls.z);
			zMax = (std::max)(zMax, ls.z);
		}
		if (zMin > zMax)
		{
			const float t = zMin;
			zMin = zMax;
			zMax = t;
		}
		nearValue = (std::min)(nearValue, zMin - kZMargin);
		farValue = (std::max)(farValue, zMax + kZMargin);
		if (nearValue >= farValue)
			farValue = nearValue + 1.0f;
	}

	static void ExpandOrthoXYForWorldAabb(const math::Matrix4x4& lightView, const math::AABB3& worldAabb, float& centerX, float& centerY, float& sizeX, float& sizeY)
	{
		math::Vector3 corners[8];
		worldAabb.GetPoint(corners);
		float lxMin = centerX - sizeX;
		float lxMax = centerX + sizeX;
		float lyMin = centerY - sizeY;
		float lyMax = centerY + sizeY;
		for (int i = 0; i < 8; ++i)
		{
			const math::Vector3 ls = lightView.TransformPosition(corners[i]);
			lxMin = (std::min)(lxMin, ls.x);
			lxMax = (std::max)(lxMax, ls.x);
			lyMin = (std::min)(lyMin, ls.y);
			lyMax = (std::max)(lyMax, ls.y);
		}
		centerX = (lxMin + lxMax) * 0.5f;
		centerY = (lyMin + lyMax) * 0.5f;
		sizeX = (lxMax - lxMin) * 0.5f * (1.0f + kXYMargin);
		sizeY = (lyMax - lyMin) * 0.5f * (1.0f + kXYMargin);
		sizeX = (std::max)(sizeX, 1e-4f);
		sizeY = (std::max)(sizeY, 1e-4f);
	}

	static void SnapLightViewTranslationToShadowTexels(math::Matrix4x4& lightView, const math::Vector3& refWorld, float sizeX, float sizeY, int shadowW, int shadowH)
	{
		const float shW = static_cast<float>((std::max)(shadowW, 1));
		const float shH = static_cast<float>((std::max)(shadowH, 1));
		const float texelLvX = (2.0f * sizeX) / shW;
		const float texelLvY = (2.0f * sizeY) / shH;
		if (texelLvX <= 1e-8f || texelLvY <= 1e-8f)
			return;
		const math::Vector3 lvRef = lightView.TransformPosition(refWorld);
		const float targetX = std::floor(lvRef.x / texelLvX + 0.5f) * texelLvX;
		const float targetY = std::floor(lvRef.y / texelLvY + 0.5f) * texelLvY;
		lightView._30 += targetX - lvRef.x;
		lightView._31 += targetY - lvRef.y;
	}

	static void RefineLightViewFromClipTexelSnap(math::Matrix4x4& lightView, const math::Matrix4x4& proj, math::Matrix4x4& outViewProj, const math::Vector3& refWorld, float sizeX,
											   float sizeY, int shadowW, int shadowH)
	{
		outViewProj = lightView * proj;
		const math::Vector4 rw(refWorld.x, refWorld.y, refWorld.z, 1.f);
		math::Vector4 clip = rw * outViewProj;
		const float iw = (std::fabs(clip.w) > 1e-8f) ? (1.f / clip.w) : 1.f;
		const float ndcX = clip.x * iw;
		const float ndcY = clip.y * iw;
		const float u = ndcX * 0.5f + 0.5f;
		const float v = ndcY * -0.5f + 0.5f;
		const float shW = static_cast<float>((std::max)(shadowW, 1));
		const float shH = static_cast<float>((std::max)(shadowH, 1));
		const float px = u * shW;
		const float py = v * shH;
		const float sx = std::floor(px + 0.5f);
		const float sy = std::floor(py + 0.5f);
		const float dPx = sx - px;
		const float dPy = sy - py;
		const float dNdcX = dPx / shW * 2.f;
		const float dNdcY = -dPy / shH * 2.f;
		lightView._30 += dNdcX * sizeX;
		lightView._31 += dNdcY * sizeY;
		outViewProj = lightView * proj;
	}

	static math::AABB3 DirectionalReceiverWorldAabbForOrthoXY(const math::AABB3& ReceiverWorldAabb, const math::AABB3& SubjectWorldAabb, const math::Vector3& LightDirWorld,
															 const Engine::FShadowProjectorSceneData& ShadowScene)
	{
		if (!ShadowScene.bHasViewWorldBoundsForDirectionalReceiverXY)
			return ReceiverWorldAabb;
		math::Vector3 L = LightDirWorld;
		if (L.GetSqrLength() > 1e-12f)
			L = L.Normalize();
		const float ly = math::Abs(L.y);
		const float grazing = math::Clamp(1.f - ly, 0.f, 1.f);
		const float casterPadWorld = 4.f + grazing * 96.f;
		const math::AABB3 casterPad = math::ExpandAabbByMargin(SubjectWorldAabb, casterPadWorld);
		math::AABB3 camVol = ShadowScene.ViewWorldBoundsAabb;
		math::AABB3 visSlice;
		if (ReceiverWorldAabb.GetIntersect(camVol, visSlice))
			return visSlice.MergeAABB(casterPad);
		return ReceiverWorldAabb.MergeAABB(casterPad);
	}

	/** Intersection of world AABB with { p | zLo <= dot(p-cam, fwd) <= zHi } (same ze convention as CSM shader splits). */
	static math::AABB3 ClipWorldAabbToCameraForwardZeSlab(const math::AABB3& box, const math::Vector3& camPos, const math::Vector3& fwdIn, float zn, float zf)
	{
		math::Vector3 fwd = fwdIn;
		if (fwd.GetSqrLength() > 1e-12f)
			fwd = fwdIn.Normalize();
		else
			fwd = math::Vector3(0.f, 0.f, 1.f);

		float zLo = zn;
		float zHi = zf;
		if (zLo > zHi)
			std::swap(zLo, zHi);

		const auto evalZe = [&](const math::Vector3& worldPt) -> float { return math::Vector3::Dot(fwd, worldPt - camPos); };

		std::vector<math::Vector3> pts;
		pts.reserve(24);

		math::Vector3 corners[8];
		box.GetPoint(corners);
		for (int i = 0; i < 8; ++i)
		{
			const float z = evalZe(corners[i]);
			if (z >= zLo - 1e-3f && z <= zHi + 1e-3f)
				pts.push_back(corners[i]);
		}

		static const int kEdgePairs[12][2] = {
			{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
		};
		for (int e = 0; e < 12; ++e)
		{
			const math::Vector3& pa = corners[kEdgePairs[e][0]];
			const math::Vector3& pb = corners[kEdgePairs[e][1]];
			const float za = evalZe(pa);
			const float zb = evalZe(pb);
			for (int pi = 0; pi < 2; ++pi)
			{
				const float planeZ = (pi == 0) ? zLo : zHi;
				if (std::fabs(za - zb) < 1e-8f)
					continue;
				const float t = (planeZ - za) / (zb - za);
				if (t < 0.f || t > 1.f)
					continue;
				pts.push_back(pa + (pb - pa) * t);
			}
		}

		if (pts.empty())
		{
			const float zMid = 0.5f * (zLo + zHi);
			const math::Vector3 c = camPos + fwd * zMid;
			const float eps = 0.05f;
			math::AABB3 out;
			out.Set(math::Vector3(c.x + eps, c.y + eps, c.z + eps), math::Vector3(c.x - eps, c.y - eps, c.z - eps));
			return out;
		}

		math::AABB3 out;
		out.Set(pts[0], pts[0]);
		for (size_t i = 1; i < pts.size(); ++i)
			out.UpdateMinMax(pts[i]);
		return out;
	}
} // namespace

namespace Engine
{
	static constexpr float kMinDirectionalLightDistance = 4.0f;

	void FDirectionalShadowFrustumFitter::FillLinearZeSplitEnds(float nearZ, float farZ, int cascadeCount, float split0Norm, float split1Norm, float outSplitZE[2])
	{
		outSplitZE[0] = 0.f;
		outSplitZE[1] = 0.f;
		if (farZ <= nearZ + 1e-5f || cascadeCount < 2)
			return;
		const float span = farZ - nearZ;
		if (cascadeCount == 2)
		{
			const float t = (std::clamp)(split0Norm, kCascadeSplitNormMin, kCascadeSplitNormMax);
			outSplitZE[0] = nearZ + span * t;
			return;
		}
		const float t0 = (std::clamp)(split0Norm, kCascadeSplitNormMin, kCascadeSplitNormMax);
		const float t1Min = t0 + kCascadeSplitPairMinGap;
		const float t1 = (std::clamp)(split1Norm, t1Min, kCascadeSplitNormMax);
		outSplitZE[0] = nearZ + span * t0;
		outSplitZE[1] = nearZ + span * t1;
	}

	math::AABB3 FDirectionalShadowFrustumFitter::ComputeCascadeSubjectWorldAabb(const FCascadeSubjectWorldAabbParams& P)
	{
		(void)P.ReceiverWorld;
		const float nearZ = P.Scene.CameraNearZ;
		const float farZ = P.Scene.CameraFarZ;
		const int cc = (std::clamp)(P.CascadeCount, 1, kMaxDirectionalCascades);
		const int ci = (std::clamp)(P.CascadeIndex, 0, cc - 1);
		const float zn = (ci <= 0) ? nearZ : P.ZeSplitBoundaries[ci - 1];
		const float zf = (ci + 1 >= cc) ? farZ : P.ZeSplitBoundaries[ci];

		math::Vector3 fwd = P.Scene.CameraForwardWorld;
		if (fwd.GetSqrLength() > 1e-12f)
			fwd = fwd.Normalize();
		else
			fwd = math::Vector3(0.f, 0.f, 1.f);

		return ClipWorldAabbToCameraForwardZeSlab(P.CasterSubjectWorld, P.Scene.CameraWorldPos, fwd, zn, zf);
	}

	void FDirectionalShadowFrustumFitter::SetupDirectionalShadowViewProjection(Light& MainLight, const FDirectionalShadowFrustumFitParams& Params)
	{
		const math::AABB3& SubjectWorldAabb = Params.SubjectWorldAabb;
		const math::AABB3& ReceiverWorldAabb = Params.ReceiverWorldAabb;
		const core::vec2i& ShadowMapSize = Params.ShadowMapSize;
		const FShadowProjectorSceneData& ShadowProjectorScene = Params.ShadowProjectorScene;
		const bool bReceiverRelativeFrustumAdjust = Params.bReceiverRelativeFrustumAdjust;
		const bool bExpandOrthoXYFromReceivers = Params.bExpandOrthoXYFromReceivers;
		const std::vector<GltfSceneMeshInfo>* SubjectMeshListForFrustum = Params.SubjectMeshListForFrustum;
		const math::AABB3* SubjectMeshWorldClipAabb = Params.SubjectMeshWorldClipAabb;

		math::Vector3 wsSceneCorners[8];
		SubjectWorldAabb.GetPoint(wsSceneCorners);

		const math::Vector3 lightLookAt = SubjectWorldAabb.GetCenter();
		math::Vector3 lightUp = math::Vector3::UnitY;
		const float subjectRadius = (std::max)(SubjectWorldAabb.GetRadius(), 0.01f);
		const float lightDistance = (std::max)(kMinDirectionalLightDistance, subjectRadius * 2.0f);
		MainLight.Position = lightLookAt + (MainLight.Direction * lightDistance);

		math::Vector3 zAxis = (lightLookAt - MainLight.Position).Normalize();
		if (math::Abs(math::Vector3::Dot(zAxis, lightUp)) > 0.999f)
			lightUp = { lightUp.z, lightUp.x, lightUp.y };

		MainLight.LightView = math::Matrix4x4::MatrixLookAtLH(MainLight.Position, lightLookAt, lightUp);

		float nearValue = 0.f;
		float farValue = 1.f;
		float centerX = 0.f;
		float centerY = 0.f;
		float sizeX = 1.f;
		float sizeY = 1.f;

		const auto fitOrthoSubject = [&]() {
			math::Vector3 lsMin{};
			math::Vector3 lsMax{};
			if (SubjectMeshListForFrustum
				&& FShadowSceneBounds::TryMergeSubjectMeshesLightSpaceExtents(SubjectMeshListForFrustum, MainLight.LightView, SubjectMeshWorldClipAabb, lsMin, lsMax))
				FitOrthoFromLightSpaceMinMax(lsMin, lsMax, nearValue, farValue, centerX, centerY, sizeX, sizeY);
			else
				FitOrthoFromWorldCorners(wsSceneCorners, MainLight.LightView, nearValue, farValue, centerX, centerY, sizeX, sizeY);
		};

		for (int iter = 0; iter < kFitSnapIterations; ++iter)
		{
			fitOrthoSubject();
			SnapLightViewTranslationToShadowTexels(MainLight.LightView, lightLookAt, sizeX, sizeY, ShadowMapSize.x, ShadowMapSize.y);
		}
		fitOrthoSubject();

		if (bReceiverRelativeFrustumAdjust)
			ExpandOrthoDepthForReceiverBounds(MainLight.LightView, ReceiverWorldAabb, nearValue, farValue);

		if (bReceiverRelativeFrustumAdjust && bExpandOrthoXYFromReceivers)
		{
			const math::AABB3 recvXY = DirectionalReceiverWorldAabbForOrthoXY(ReceiverWorldAabb, SubjectWorldAabb, MainLight.Direction, ShadowProjectorScene);
			ExpandOrthoXYForWorldAabb(MainLight.LightView, recvXY, centerX, centerY, sizeX, sizeY);
		}

		const math::Matrix4x4 proj = math::Matrix4x4::MatrixOrthographicOffCenterLH(centerX - sizeX, centerX + sizeX, centerY - sizeY, centerY + sizeY, nearValue, farValue);
		RefineLightViewFromClipTexelSnap(MainLight.LightView, proj, MainLight.LightViewProj, lightLookAt, sizeX, sizeY, ShadowMapSize.x, ShadowMapSize.y);
	}

} // namespace Engine
