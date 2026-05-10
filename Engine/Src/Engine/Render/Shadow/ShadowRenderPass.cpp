#include "Render/Shadow/ShadowRenderPass.h"
#include "Render/MaterialPreFrame.h"
#include "GltfModel/GltfMesh.h"
#include "Material/FurMaterial.h"
#include "Scene/SceneMeshComponent.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICommandContext.h"
#include "Render/Shadow/ShadowPS.h"
#include "Render/Shadow/ShadowMapManager.h"
#include "Render/Shadow/ShadowMap.h"
#include "RHI/RHIRenderTarget.h"
#include "RHI/RHITextureCube.h"
#include "math/aabb3.h"
#include "math/math.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include <cmath>
#include <vector>
#include <cfloat>
#include <unordered_set>

namespace
{
	/**
	 * ShadowPass-PS: opaque and MASK paths output depth unchanged. BLEND without sampling would paint a solid depth hull;
	 * when a base-color map exists we compile SHADOW_ALPHA_CLIP and discard by texture alpha (see ShadowPass-PS.hlsl).
	 */
	static bool MeshWritesShadowMapDepth(const std::shared_ptr<Engine::MeshBase>& Mesh)
	{
		if (!Mesh)
			return false;
		const auto mat = Mesh->GetMaterial();
		if (!mat)
			return false;
		if (!mat->IsTransparent())
			return true;
		return mat->GetBaseColorTexture() != nullptr;
	}

	// When true: fit orthographic shadow XY to shadow casters only (keeps texel density on the character).
	// Visible receiver bounds (FrustumBoundsMeshes) still contribute to light-space Z so ground contact shadows are not clipped.
	static constexpr bool kPreferTightShadowFrustumFromCasters = true;

	static constexpr float kZMargin = 4.0f;
	static constexpr float kXYMargin = 0.14f;
	// Fit / snap iterations: snap changes LightView, which changes light-space bounds; need another fit+snap with updated half-extents.
	static constexpr int kFitSnapIterations = 2;

	static void FitOrthoFromWorldCorners(
		const math::Vector3 wsSceneCorners[8],
		const math::Matrix4x4& lightView,
		float& nearValue,
		float& farValue,
		float& centerX,
		float& centerY,
		float& sizeX,
		float& sizeY)
	{
		math::Vector3 lsMin(FLT_MAX, FLT_MAX, FLT_MAX);
		math::Vector3 lsMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (int i = 0; i < 8; ++i)
		{
			math::Vector3 lsCorner = lightView.TransformPosition(wsSceneCorners[i]);
			lsMin = math::Vector3((std::min)(lsMin.x, lsCorner.x), (std::min)(lsMin.y, lsCorner.y), (std::min)(lsMin.z, lsCorner.z));
			lsMax = math::Vector3((std::max)(lsMax.x, lsCorner.x), (std::max)(lsMax.y, lsCorner.y), (std::max)(lsMax.z, lsCorner.z));
		}
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

	static math::AABB3 WorldMeshBoundsForShadowFrustum(const std::shared_ptr<Engine::MeshBase>& Mesh, const math::AABB3& TransformedMeshBox)
	{
		if (!Mesh || !Mesh->GetMaterial())
			return TransformedMeshBox;
		auto FurMat = std::dynamic_pointer_cast<Engine::FurMaterial>(Mesh->GetMaterial());
		if (!FurMat)
			return TransformedMeshBox;
		const float Reach = (std::max)(0.f, FurMat->GetFurConfig().FurLength) * 1.2f;
		return math::ExpandAabbByMargin(TransformedMeshBox, Reach);
	}

	// When kPreferTightShadowFrustumFromCasters: XY (and baseline Z) come from caster AABB only.
	// Extend light-space Z to include visible receivers (ground); otherwise contact shadows on the floor
	// clip at a hard frustum boundary -> ear/body shadows look "broken" along a box edge.
	static void ExpandOrthoDepthForReceiverBounds(const math::Matrix4x4& lightView, const math::AABB3& receiverWorldAabb,
												float& nearValue, float& farValue)
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

	// Widen ortho XY to include receiver world corners (merged into current light-space rect).
	// Do not use only receiver∩caster: at low sun elevation shadows stretch far across the ground outside the caster AABB,
	// and intersecting AABBs clips that region -> hard shadow boundary ("broken" edge).
	static void ExpandOrthoXYForWorldAabb(const math::Matrix4x4& lightView, const math::AABB3& worldAabb, float& centerX, float& centerY,
										  float& sizeX, float& sizeY)
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

	static void SnapLightViewTranslationToShadowTexels(
		math::Matrix4x4& lightView,
		const math::Vector3& refWorld,
		float sizeX,
		float sizeY,
		int32_t shadowW,
		int32_t shadowH)
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

	// After View*Proj, nudge LightView so the reference point projects to integer shadow-map pixels (same UV remap as deferred PCSS).
	static void RefineLightViewFromClipTexelSnap(
		math::Matrix4x4& lightView,
		const math::Matrix4x4& proj,
		math::Matrix4x4& outViewProj,
		const math::Vector3& refWorld,
		float sizeX,
		float sizeY,
		int32_t shadowW,
		int32_t shadowH)
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
}

namespace Engine
{
	// Note: glTFSample derives directional-light "position" from a scene node transform.
	// Our engine does not have a directional-light node position, so for shadow view we
	// place the light along its direction far enough to stably cover the subject bounds.
	// Using a fixed small distance breaks across different scene scales.
	static constexpr float kMinDirectionalLightDistance = 4.0f;
	static constexpr int32_t kPointShadowCubeSize = 512;

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

	struct ShadowRenderPassPrivate
	{
		RenderCore::DynamicRHI* RHI;
		std::map<std::shared_ptr<MeshBase>, std::shared_ptr<ShadowPS>> ShadowRenders;
		std::shared_ptr<ShadowMapManager> ShadowMgr;
		std::shared_ptr<RenderCore::RHIRenderTarget> DepthRenderBuffer;
		std::shared_ptr<RenderCore::RHIRenderTarget> SpotShadowBuffer;
		std::shared_ptr<RenderCore::RHITextureCube> PointShadowCube;
		Light CachedMainLightForShading{};
		bool bCachedMainLightValid = false;
		int CachedMainDirectionalShadowLightListIndex = -1;

		math::Matrix4x4 CachedPointFaceVP[6]{};
		math::Vector3 CachedPointLightPos{};
		float CachedPointLightRange = 0.f;
		int CachedPointShadowLightIndex = -1;
		bool bCachedPointShadowValid = false;

		math::Matrix4x4 CachedSpotLightViewProj{};
		math::Matrix4x4 CachedSpotLightView{};
		int CachedSpotShadowLightIndex = -1;
		bool bCachedSpotShadowValid = false;

		CBDirectionalShadowCSM CachedDirectionalCSM{};
		bool bCachedDirectionalCSMParamsValid = false;

		ShadowRenderPassPrivate(RenderCore::DynamicRHI* _RHI)
			: RHI(_RHI)
		{
			ShadowMgr = std::make_shared<ShadowMapManager>();
			ShadowMgr->SetShadowCascades(1);
		}
	};

	static void UpdateShadowPSPaletteForMesh(const std::shared_ptr<ShadowPS>& shadowRender, const std::shared_ptr<MeshBase>& Mesh)
	{
		if (!shadowRender || !Mesh || !Mesh->HasSkin())
			return;
		const bool bResolvedPalette = Mesh->GetSkinId() > -1 && !Mesh->GetBoneNodeArray().empty()
			&& Mesh->GetSkinId() < static_cast<int>(Mesh->GetBoneNodeArray().size());
		if (bResolvedPalette)
		{
			auto& Bone = Mesh->GetBoneNodeArray()[static_cast<size_t>(Mesh->GetSkinId())];
			const uint32_t MaxSkin = static_cast<uint32_t>(CBPerSkeleton::kPaletteMatrixCount);
			const uint32_t NumBones = static_cast<uint32_t>(Bone.size());
			for (uint32_t BoneIndex = 0; BoneIndex < NumBones && BoneIndex < MaxSkin; ++BoneIndex)
				shadowRender->SetBoneMatrix(Bone[BoneIndex].FinalMat, static_cast<int32_t>(BoneIndex));
		}
		else
			shadowRender->ResetSkeletonPaletteIdentity();
	}

	static void DrawShadowCasterMeshesDirectional(
		ShadowRenderPassPrivate* d,
		RenderCore::RHICommandContext& RHIContext,
		const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes,
		const Light& light,
		const std::shared_ptr<RenderCore::RHIRenderTarget>& rt)
	{
		for (const auto& MeshInfo : ShadowCasterMeshes)
		{
			for (size_t MeshIndex = 0; MeshIndex < MeshInfo.Meshes.size(); ++MeshIndex)
			{
				std::shared_ptr<MeshBase> Mesh = MeshInfo.Meshes[MeshIndex];
				if (!Mesh || !MeshWritesShadowMapDepth(Mesh))
					continue;
				auto& shadowRender = d->ShadowRenders[Mesh];
				if (!shadowRender)
				{
					shadowRender = std::make_shared<ShadowPS>(d->RHI, Mesh);
					shadowRender->InitResource();
				}
				UpdateShadowPSPaletteForMesh(shadowRender, Mesh);
				shadowRender->Draw(RHIContext, Mesh->GetMeshMat() * MeshInfo.WorldTransform, light, rt);
			}
		}
	}

	static void DrawShadowCasterMeshesPointCubeFace(
		ShadowRenderPassPrivate* d,
		RenderCore::RHICommandContext& RHIContext,
		const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes,
		const Light& faceLight,
		const std::shared_ptr<RenderCore::RHITextureCube>& cube,
		int face)
	{
		for (const auto& MeshInfo : ShadowCasterMeshes)
		{
			for (size_t MeshIndex = 0; MeshIndex < MeshInfo.Meshes.size(); ++MeshIndex)
			{
				std::shared_ptr<MeshBase> Mesh = MeshInfo.Meshes[MeshIndex];
				if (!Mesh || !MeshWritesShadowMapDepth(Mesh))
					continue;
				auto& shadowRender = d->ShadowRenders[Mesh];
				if (!shadowRender)
				{
					shadowRender = std::make_shared<ShadowPS>(d->RHI, Mesh);
					shadowRender->InitResource();
				}
				UpdateShadowPSPaletteForMesh(shadowRender, Mesh);
				shadowRender->DrawCubeFace(RHIContext, Mesh->GetMeshMat() * MeshInfo.WorldTransform, faceLight, cube, face);
			}
		}
	}

	/** Orthographic cascaded/map shadow applies to the **first** directional only (`GatherLightsForView` lists all dirs first). */
	static int FindFirstDirectionalLightIndex(const std::vector<Light>& Lights)
	{
		for (int i = 0; i < static_cast<int>(Lights.size()); ++i)
		{
			if (Lights[static_cast<size_t>(i)].Type == LightType_Directional)
				return i;
		}
		return -1;
	}

	/**
	 * One cubemap shadow target: first point light that requested cube shadow (`ShadowMapIndex == kPointLightCubeShadowMapIndex`).
	 * Any number of point lights still contribute analytic shading; only this index pairs with `PointShadowLightIndex` in deferred.
	 */
	static int FindPointShadowCubeLightIndex(const std::vector<Light>& Lights)
	{
		for (int i = 0; i < static_cast<int>(Lights.size()); ++i)
		{
			const Light& L = Lights[static_cast<size_t>(i)];
			if (L.Type == LightType_Point && L.ShadowMapIndex == kPointLightCubeShadowMapIndex)
				return i;
		}
		return -1;
	}

	/**
	 * One spotlight depth texture: first spot with `CastShadow` (`ShadowMapIndex == kSpotLightShadowMapIndex`).
	 * Order follows `GatherLightsForView` (spots sorted by component SortPriority, descending). Others stay lit without this depth map.
	 */
	static int FindSpotShadowLightIndex(const std::vector<Light>& Lights)
	{
		for (int i = 0; i < static_cast<int>(Lights.size()); ++i)
		{
			const Light& L = Lights[static_cast<size_t>(i)];
			if (L.Type == LightType_Spot && L.ShadowMapIndex == kSpotLightShadowMapIndex)
				return i;
		}
		return -1;
	}

	/** Max positive distance along spot axis (eye→scene) to AABB corners; drives zFar when lamp is far (procedural sun). */
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

	static void SetupSpotShadowViewProjection(Light& spotLight, const math::AABB3* pSceneBoundsWorld, bool bSceneBoundsValid)
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
		// KHR negative range = infinite attenuation in deferred; shadow map still needs a finite clip far.
		// Procedural "sun" spots sit far along the ray — fixed zFar or Range-only zFar can clip receivers before the map sees depth.
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

	/** Mesh list that drives orthographic frustum fitting (UE-ish "subject bounds" source vs full receiver set). */
	static const std::vector<GltfSceneMeshInfo>* SelectShadowSubjectMeshListForFrustum(const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const std::vector<GltfSceneMeshInfo>& FrustumBoundsMeshes,
																					   const FShadowProjectorSceneData& ShadowProjectorScene)
	{
		if (kPreferTightShadowFrustumFromCasters)
		{
			if (!ShadowCasterMeshes.empty())
				return &ShadowCasterMeshes;
			if (ShadowProjectorScene.bValid)
				return nullptr;
			if (!FrustumBoundsMeshes.empty())
				return &FrustumBoundsMeshes;
			return &ShadowCasterMeshes;
		}
		return !FrustumBoundsMeshes.empty() ? &FrustumBoundsMeshes : &ShadowCasterMeshes;
	}

	static void PruneStaleMeshShadowPS(ShadowRenderPassPrivate* d, const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes,
									   const std::vector<GltfSceneMeshInfo>& FrustumBoundsMeshes)
	{
		std::unordered_set<const MeshBase*> casterMeshPtrs;
		auto insertMeshes = [&casterMeshPtrs](const std::vector<GltfSceneMeshInfo>& List) {
			for (const auto& MeshInfo : List)
			{
				for (const auto& Mesh : MeshInfo.Meshes)
				{
					if (Mesh)
						casterMeshPtrs.insert(Mesh.get());
				}
			}
		};
		insertMeshes(ShadowCasterMeshes);
		insertMeshes(FrustumBoundsMeshes);
		for (auto it = d->ShadowRenders.begin(); it != d->ShadowRenders.end();)
		{
			if (!it->first || casterMeshPtrs.find(it->first.get()) == casterMeshPtrs.end())
				it = d->ShadowRenders.erase(it);
			else
				++it;
		}
	}

	/** World-space union AABB for shadow "subject" geometry (casters / frustum driver), including fur shell margin; projector fallback if empty. */
	static void BuildMergedShadowSubjectWorldAabb(const std::vector<GltfSceneMeshInfo>* SubjectMeshList, const FShadowProjectorSceneData& ShadowProjectorScene, math::AABB3& OutSubjectWorldAabb,
												  bool& OutSubjectValid)
	{
		OutSubjectValid = false;
		if (SubjectMeshList)
		{
			for (const auto& MeshInfo : *SubjectMeshList)
			{
				for (const auto& Mesh : MeshInfo.Meshes)
				{
					if (!Mesh || !MeshWritesShadowMapDepth(Mesh))
						continue;
					math::AABB3 wbox = Mesh->GetBoundingBox().Transform(MeshInfo.WorldTransform);
					wbox = WorldMeshBoundsForShadowFrustum(Mesh, wbox);
					OutSubjectWorldAabb = OutSubjectValid ? OutSubjectWorldAabb.MergeAABB(wbox) : wbox;
					OutSubjectValid = true;
				}
			}
		}
		if (!OutSubjectValid && ShadowProjectorScene.bValid)
		{
			OutSubjectWorldAabb = ShadowProjectorScene.ModelLocalAABB.Transform(ShadowProjectorScene.WorldTransform);
			OutSubjectValid = true;
		}
	}

	static void BuildMergedShadowReceiverWorldAabb(const std::vector<GltfSceneMeshInfo>& FrustumBoundsMeshes, math::AABB3& OutReceiverWorldAabb, bool& OutReceiverValid)
	{
		OutReceiverValid = false;
		for (const auto& MeshInfo : FrustumBoundsMeshes)
		{
			for (const auto& Mesh : MeshInfo.Meshes)
			{
				if (!Mesh)
					continue;
				math::AABB3 wbox = Mesh->GetBoundingBox().Transform(MeshInfo.WorldTransform);
				OutReceiverWorldAabb = OutReceiverValid ? OutReceiverWorldAabb.MergeAABB(wbox) : wbox;
				OutReceiverValid = true;
			}
		}
	}

	// Optional primary-view AABB (FShadowProjectorSceneData::ViewWorldBoundsAabb): receiver ∩ view hull + grazing-padded caster for ortho XY.
	static math::AABB3 DirectionalReceiverWorldAabbForOrthoXY(const math::AABB3& ReceiverWorldAabb, const math::AABB3& SubjectWorldAabb, const math::Vector3& LightDirWorld,
															  const FShadowProjectorSceneData& ShadowScene)
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

	static constexpr int kDirectionalCSMCount = 3;
	static constexpr int kCascadeShadowResolution = 2048;
	static constexpr float kCSMSplitLambda = 0.82f;

	static void ComputeDirectionalCascadeSplitEnds(float n, float f, float outEnds[kDirectionalCSMCount])
	{
		const float fn = std::max(f, n + 1e-2f);
		for (int i = 0; i < kDirectionalCSMCount; ++i)
		{
			const float ratio = static_cast<float>(i + 1) / static_cast<float>(kDirectionalCSMCount);
			const float logd = n * std::pow(fn / n, ratio);
			const float unid = n + (fn - n) * ratio;
			outEnds[i] = kCSMSplitLambda * logd + (1.f - kCSMSplitLambda) * unid;
		}
	}

	static math::AABB3 WorldBoundsFromViewProjSliceInverse(const math::Matrix4x4& CameraView, float fovy, float aspectWH, float zn, float zf)
	{
		const math::Matrix4x4 proj = math::Matrix4x4::MatrixPerspectiveFovLH(fovy, aspectWH, zn, zf);
		const math::Matrix4x4 vp = CameraView * proj;
		const math::Matrix4x4 invVP = vp.Inverse();
		std::vector<math::Vector3> pts;
		pts.reserve(8);
		const float sx[] = { -1.f, 1.f };
		const float sy[] = { -1.f, 1.f };
		const float sz[] = { 0.f, 1.f };
		for (float x : sx)
			for (float y : sy)
				for (float z : sz)
				{
					const math::Vector4 clip(x, y, z, 1.f);
					const math::Vector4 wh = clip * invVP;
					const float iw = (std::fabs(wh.w) > 1e-8f) ? (1.f / wh.w) : 1.f;
					pts.emplace_back(wh.x * iw, wh.y * iw, wh.z * iw);
				}
		math::AABB3 box;
		box.CreateAABB(pts);
		return box;
	}

	/** Directional shadow depth pass view-projection; optional receiver-aware XY/Z when using tight caster frustum. */
	static void SetupDirectionalShadowViewProjection(Light& MainLight, const math::AABB3& SubjectWorldAabb, bool bReceiverRelativeFrustumAdjust, const math::AABB3& ReceiverWorldAabb,
													 const core::vec2i& ShadowMapSize, const FShadowProjectorSceneData& ShadowProjectorScene,
													 bool bExpandOrthoXYFromReceivers)
	{
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

		for (int iter = 0; iter < kFitSnapIterations; ++iter)
		{
			FitOrthoFromWorldCorners(wsSceneCorners, MainLight.LightView, nearValue, farValue, centerX, centerY, sizeX, sizeY);
			SnapLightViewTranslationToShadowTexels(MainLight.LightView, lightLookAt, sizeX, sizeY, ShadowMapSize.x, ShadowMapSize.y);
		}
		FitOrthoFromWorldCorners(wsSceneCorners, MainLight.LightView, nearValue, farValue, centerX, centerY, sizeX, sizeY);

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

	static void RenderDirectionalShadowMapPass(ShadowRenderPassPrivate* d, RenderCore::RHICommandContext& RHIContext, const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const Light& MainLight)
	{
		RHIContext.Clear(d->DepthRenderBuffer, core::FLinearColor::White, 1.f, 0);
		const auto TargetSize = d->DepthRenderBuffer->GetSize();
		RHIContext.SetViewPort(0, 0, TargetSize.x, TargetSize.y);
		DrawShadowCasterMeshesDirectional(d, RHIContext, ShadowCasterMeshes, MainLight, d->DepthRenderBuffer);
	}

	static void RenderSpotShadowMapPass(ShadowRenderPassPrivate* d, RenderCore::RHICommandContext& RHIContext, const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const Light& SpotLight)
	{
		RHIContext.Clear(d->SpotShadowBuffer, core::FLinearColor::White, 1.f, 0);
		const auto TargetSize = d->SpotShadowBuffer->GetSize();
		RHIContext.SetViewPort(0, 0, TargetSize.x, TargetSize.y);
		DrawShadowCasterMeshesDirectional(d, RHIContext, ShadowCasterMeshes, SpotLight, d->SpotShadowBuffer);
	}

	static void RenderPointLightShadowCubePass(ShadowRenderPassPrivate* d, RenderCore::RHICommandContext& RHIContext, const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const Light& PointLight,
											   int PointLightIndex)
	{
		const float zNear = 0.05f;
		const float zFar = (std::max)(PointLight.Range, zNear + 0.1f);
		for (int face = 0; face < 6; ++face)
			d->CachedPointFaceVP[face] = ComputePointShadowFaceViewProj(PointLight.Position, face, zNear, zFar);
		d->CachedPointLightPos = PointLight.Position;
		d->CachedPointLightRange = PointLight.Range;
		d->CachedPointShadowLightIndex = PointLightIndex;

		const core::vec2i cubeSize = d->PointShadowCube->GetSize();
		for (int face = 0; face < 6; ++face)
		{
			RHIContext.Clear(d->PointShadowCube, face, 0, core::FLinearColor::White, 1.f, 0);
			RHIContext.SetViewPort(0, 0, cubeSize.x, cubeSize.y);
			Light faceLight = PointLight;
			faceLight.LightViewProj = d->CachedPointFaceVP[face];
			DrawShadowCasterMeshesPointCubeFace(d, RHIContext, ShadowCasterMeshes, faceLight, d->PointShadowCube, face);
		}
		d->bCachedPointShadowValid = true;
	}

	ShadowRenderPass::ShadowRenderPass(RenderCore::DynamicRHI* RHI)
		: d_ptr(new ShadowRenderPassPrivate(RHI))
	{
	}

	ShadowRenderPass::~ShadowRenderPass()
	{
		delete d_ptr;
	}

	void ShadowRenderPass::InitResource()
	{
		C_P(ShadowRenderPass);
		const int32_t SHADOW_WIDTH = kCascadeShadowResolution;
		const int32_t SHADOW_HEIGHT = kCascadeShadowResolution * kDirectionalCSMCount;
		d->DepthRenderBuffer = d->RHI->RHICreateRenderTarget(RenderCore::EPixelFormat::PF_R32_FLOAT, SHADOW_WIDTH, SHADOW_HEIGHT, 1, false, true);
		if (!d->PointShadowCube)
			d->PointShadowCube = d->RHI->RHICreateTextureCube(RenderCore::EPixelFormat::PF_R32_FLOAT, kPointShadowCubeSize, kPointShadowCubeSize, 1, false);
		const int32_t kSpotShadowSize = 2048;
		if (!d->SpotShadowBuffer)
			d->SpotShadowBuffer = d->RHI->RHICreateRenderTarget(RenderCore::EPixelFormat::PF_R32_FLOAT, kSpotShadowSize, kSpotShadowSize, 1, false, true);
	}

	void ShadowRenderPass::InvalidateCachedMainLightForShading()
	{
		C_P(ShadowRenderPass);
		d->bCachedMainLightValid = false;
		d->CachedMainDirectionalShadowLightListIndex = -1;
		d->bCachedDirectionalCSMParamsValid = false;
		d->bCachedPointShadowValid = false;
		d->bCachedSpotShadowValid = false;
	}

	bool ShadowRenderPass::TryGetCachedMainLightForShading(Light& OutLight, int* OutLightListIndexInLastShadowPassLights)
	{
		C_P(ShadowRenderPass);
		if (!d->bCachedMainLightValid)
			return false;
		OutLight = d->CachedMainLightForShading;
		if (OutLightListIndexInLastShadowPassLights)
			*OutLightListIndexInLastShadowPassLights = d->CachedMainDirectionalShadowLightListIndex;
		return true;
	}

	bool ShadowRenderPass::TryGetCachedDirectionalCSM(CBDirectionalShadowCSM& Out) const
	{
		C_P(const ShadowRenderPass);
		if (!d->bCachedDirectionalCSMParamsValid)
			return false;
		Out = d->CachedDirectionalCSM;
		return true;
	}

	void ShadowRenderPass::ClearCachedMeshShadowPasses()
	{
		C_P(ShadowRenderPass);
		d->ShadowRenders.clear();
	}

	void ShadowRenderPass::Render(const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const std::vector<GltfSceneMeshInfo>& FrustumBoundsMeshes,
								  RenderCore::RHICommandContext& RHIContext, std::vector<Light> Lights, const FShadowProjectorSceneData& ShadowProjectorScene)
	{
		C_P(ShadowRenderPass);
		d->bCachedMainLightValid = false;
		d->CachedMainDirectionalShadowLightListIndex = -1;
		d->bCachedDirectionalCSMParamsValid = false;
		d->CachedDirectionalCSM = CBDirectionalShadowCSM{};
		d->bCachedPointShadowValid = false;
		d->bCachedSpotShadowValid = false;
		d->ShadowMgr->Update(Lights, ShadowProjectorScene);

		// FrustumBoundsMeshes may contain receivers/floor when caster list is empty; still needed for spot depth + ortho fitting.
		if (ShadowCasterMeshes.empty() && FrustumBoundsMeshes.empty() && !ShadowProjectorScene.bValid)
			return;

		const std::vector<GltfSceneMeshInfo>* subjectMeshList = SelectShadowSubjectMeshListForFrustum(ShadowCasterMeshes, FrustumBoundsMeshes, ShadowProjectorScene);
		PruneStaleMeshShadowPS(d, ShadowCasterMeshes, FrustumBoundsMeshes);

		const int mainDirIdx = FindFirstDirectionalLightIndex(Lights);
		const int pointShadowIdx = FindPointShadowCubeLightIndex(Lights);
		const int spotShadowIdx = FindSpotShadowLightIndex(Lights);

		math::AABB3 subjectWorldAabb;
		bool subjectValid = false;
		BuildMergedShadowSubjectWorldAabb(subjectMeshList, ShadowProjectorScene, subjectWorldAabb, subjectValid);

		math::AABB3 receiverWorldAabb;
		bool receiverValid = false;
		BuildMergedShadowReceiverWorldAabb(FrustumBoundsMeshes, receiverWorldAabb, receiverValid);

		if (mainDirIdx >= 0 && subjectValid)
		{
			Light& mainLightRef = Lights[static_cast<size_t>(mainDirIdx)];
			mainLightRef.ShadowMapIndex = 0;
			const core::vec2i fullTexSize = d->DepthRenderBuffer ? d->DepthRenderBuffer->GetSize()
																 : core::vec2i{ kCascadeShadowResolution, kCascadeShadowResolution * kDirectionalCSMCount };
			const core::vec2i cascadeTexSize{ kCascadeShadowResolution, kCascadeShadowResolution };
			const bool bReceiverRelativeFrustumAdjust = receiverValid && kPreferTightShadowFrustumFromCasters && subjectMeshList == &ShadowCasterMeshes;

			d->bCachedDirectionalCSMParamsValid = true;
			d->CachedDirectionalCSM = CBDirectionalShadowCSM{};
			d->CachedDirectionalCSM.DirectionalCSMEnabled = 0;

			if (ShadowProjectorScene.bHasCascadeCameraParams)
			{
				float splitEnds[kDirectionalCSMCount];
				ComputeDirectionalCascadeSplitEnds(ShadowProjectorScene.CameraNearZ, ShadowProjectorScene.CameraFarZ, splitEnds);
				d->CachedDirectionalCSM.DirectionalCSMEnabled = 1;
				d->CachedDirectionalCSM.CascadeSplits =
					math::Vector4(splitEnds[0], splitEnds[1], ShadowProjectorScene.CameraFarZ, static_cast<float>(kDirectionalCSMCount));
				const float invN = 1.f / static_cast<float>(kDirectionalCSMCount);
				d->CachedDirectionalCSM.CameraForwardInvCount =
					math::Vector4(ShadowProjectorScene.CameraForwardWorld.x, ShadowProjectorScene.CameraForwardWorld.y,
								  ShadowProjectorScene.CameraForwardWorld.z, invN);

				RHIContext.Clear(d->DepthRenderBuffer, core::FLinearColor::White, 1.f, 0);

				const float camNear = ShadowProjectorScene.CameraNearZ;
				Light firstCascadeLight{};
				for (int ci = 0; ci < kDirectionalCSMCount; ++ci)
				{
					const float zNearSlice = (ci == 0) ? camNear : splitEnds[ci - 1];
					const float zFarSlice = splitEnds[ci];
					const math::AABB3 sliceBounds =
						WorldBoundsFromViewProjSliceInverse(ShadowProjectorScene.CameraView, ShadowProjectorScene.CameraFovYRad,
														   ShadowProjectorScene.CameraAspectWH, zNearSlice, zFarSlice);
					const math::AABB3 cascadeSubject = subjectWorldAabb.MergeAABB(sliceBounds);

					Light Li = mainLightRef;
					SetupDirectionalShadowViewProjection(Li, cascadeSubject, bReceiverRelativeFrustumAdjust, receiverWorldAabb, cascadeTexSize,
														 ShadowProjectorScene, false);
					d->CachedDirectionalCSM.CascadeViewProj[ci] = Li.LightViewProj;
					if (ci == 0)
						firstCascadeLight = Li;

					RHIContext.SetViewPort(0, ci * kCascadeShadowResolution, kCascadeShadowResolution, kCascadeShadowResolution);
					DrawShadowCasterMeshesDirectional(d, RHIContext, ShadowCasterMeshes, Li, d->DepthRenderBuffer);
				}
				d->CachedMainLightForShading = firstCascadeLight;
				d->CachedMainLightForShading.ShadowMapIndex = 0;
				d->CachedMainDirectionalShadowLightListIndex = mainDirIdx;
				d->bCachedMainLightValid = true;
			}
			else
			{
				SetupDirectionalShadowViewProjection(mainLightRef, subjectWorldAabb, bReceiverRelativeFrustumAdjust, receiverWorldAabb, fullTexSize,
													 ShadowProjectorScene, true);
				d->CachedMainLightForShading = mainLightRef;
				d->CachedMainDirectionalShadowLightListIndex = mainDirIdx;
				d->bCachedMainLightValid = true;
				RHIContext.SetViewPort(0, 0, fullTexSize.x, fullTexSize.y);
				RenderDirectionalShadowMapPass(d, RHIContext, ShadowCasterMeshes, mainLightRef);
			}
		}

		if (pointShadowIdx >= 0 && d->PointShadowCube && !ShadowCasterMeshes.empty())
			RenderPointLightShadowCubePass(d, RHIContext, ShadowCasterMeshes, Lights[static_cast<size_t>(pointShadowIdx)], pointShadowIdx);

		// Spot depth pass must rasterize occluder geometry. DynamicShadowCastingPrimitives can be empty in edge cases
		// (registration / cull ordering) while ShadowFrustumCullPrimitives still has the same meshes — fall back so spot
		// shadows are not silently skipped (harley.obj + procedural sky scenes).
		if (spotShadowIdx >= 0 && d->SpotShadowBuffer)
		{
			const std::vector<GltfSceneMeshInfo>& spotMeshList =
				!ShadowCasterMeshes.empty() ? ShadowCasterMeshes : FrustumBoundsMeshes;
			if (!spotMeshList.empty())
			{
				Light& spotL = Lights[static_cast<size_t>(spotShadowIdx)];
				math::AABB3 spotZFarBounds{};
				bool spotZFarOk = false;
				if (subjectValid)
				{
					spotZFarBounds = subjectWorldAabb;
					spotZFarOk = true;
				}
				if (receiverValid)
				{
					spotZFarBounds = spotZFarOk ? spotZFarBounds.MergeAABB(receiverWorldAabb) : receiverWorldAabb;
					spotZFarOk = true;
				}
				SetupSpotShadowViewProjection(spotL, spotZFarOk ? &spotZFarBounds : nullptr, spotZFarOk);
				d->CachedSpotLightViewProj = spotL.LightViewProj;
				d->CachedSpotLightView = spotL.LightView;
				d->CachedSpotShadowLightIndex = spotShadowIdx;
				d->bCachedSpotShadowValid = true;
				RenderSpotShadowMapPass(d, RHIContext, spotMeshList, spotL);
			}
		}

		// Directional CSM leaves RSSetViewports at atlas tiles (non-zero TopLeftY). Reset so nothing downstream
		// (or same-context helpers) assumes viewport origin (0,0) without rebinding.
		if (d->DepthRenderBuffer)
		{
			const core::vec2i ds = d->DepthRenderBuffer->GetSize();
			RHIContext.SetViewPort(0, 0, ds.x, ds.y);
		}
	}

	std::shared_ptr<RenderCore::RHIRenderTarget> ShadowRenderPass::GetShadowMap() const
	{
		C_P(ShadowRenderPass);
		return d->DepthRenderBuffer;
	}

	std::shared_ptr<RenderCore::RHITextureCube> ShadowRenderPass::GetPointShadowCube() const
	{
		C_P(ShadowRenderPass);
		return d->PointShadowCube;
	}

	bool ShadowRenderPass::TryGetCachedPointShadowForDeferred(int& OutLightIndex, math::Matrix4x4 OutFaceVp[6], math::Vector4& OutPosRange) const
	{
		C_P(ShadowRenderPass);
		if (!d->bCachedPointShadowValid)
			return false;
		for (int i = 0; i < 6; ++i)
			OutFaceVp[i] = d->CachedPointFaceVP[i];
		OutLightIndex = d->CachedPointShadowLightIndex;
		OutPosRange = math::Vector4(d->CachedPointLightPos.x, d->CachedPointLightPos.y, d->CachedPointLightPos.z, d->CachedPointLightRange);
		return true;
	}

	std::shared_ptr<RenderCore::RHIRenderTarget> ShadowRenderPass::GetSpotShadowMap() const
	{
		C_P(ShadowRenderPass);
		return d->SpotShadowBuffer;
	}

	bool ShadowRenderPass::TryGetCachedSpotShadowForDeferred(int& OutLightIndex, math::Matrix4x4& OutSpotLightViewProj, math::Matrix4x4* OutOptionalLightView) const
	{
		C_P(ShadowRenderPass);
		if (!d->bCachedSpotShadowValid)
			return false;
		OutLightIndex = d->CachedSpotShadowLightIndex;
		OutSpotLightViewProj = d->CachedSpotLightViewProj;
		if (OutOptionalLightView)
			*OutOptionalLightView = d->CachedSpotLightView;
		return true;
	}

} // namespace Engine
