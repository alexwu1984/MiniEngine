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
#include "math/vector4.h"
#include <cmath>
#include <cfloat>
#include <unordered_set>

namespace
{
	/** ShadowPass-PS outputs depth without alpha test; BLEND materials would write solid silhouette depth for fade meshes. */
	static bool MeshWritesShadowMapDepth(const std::shared_ptr<Engine::MeshBase>& Mesh)
	{
		if (!Mesh)
			return false;
		const auto mat = Mesh->GetMaterial();
		return mat && !mat->IsTransparent();
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
	// clip at a hard frustum boundary → ear/body shadows look "broken" along a box edge.
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

	// Widen ortho XY using receiver geometry near the caster (intersection with padded caster AABB).
	// Otherwise top-down views: ear shadows on the floor extend in light-space XY past the caster-only fit → hard clip (horizontal "break").
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
	static constexpr float LIGHT_DISTANCE = 4.0f;
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
		std::shared_ptr<RenderCore::RHITextureCube> PointShadowCube;
		Light CachedMainLightForShading{};
		bool bCachedMainLightValid = false;

		math::Matrix4x4 CachedPointFaceVP[6]{};
		math::Vector3 CachedPointLightPos{};
		float CachedPointLightRange = 0.f;
		int CachedPointShadowLightIndex = -1;
		bool bCachedPointShadowValid = false;

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

	static int FindFirstDirectionalLightIndex(const std::vector<Light>& Lights)
	{
		for (int i = 0; i < static_cast<int>(Lights.size()); ++i)
		{
			if (Lights[static_cast<size_t>(i)].Type == LightType_Directional)
				return i;
		}
		return -1;
	}

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

	/** Mesh list that drives orthographic frustum fitting (UE-ish “subject bounds” source vs full receiver set). */
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

	static void PruneStaleMeshShadowPS(ShadowRenderPassPrivate* d, const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes)
	{
		std::unordered_set<const MeshBase*> casterMeshPtrs;
		for (const auto& MeshInfo : ShadowCasterMeshes)
		{
			for (const auto& Mesh : MeshInfo.Meshes)
			{
				if (Mesh)
					casterMeshPtrs.insert(Mesh.get());
			}
		}
		for (auto it = d->ShadowRenders.begin(); it != d->ShadowRenders.end();)
		{
			if (!it->first || casterMeshPtrs.find(it->first.get()) == casterMeshPtrs.end())
				it = d->ShadowRenders.erase(it);
			else
				++it;
		}
	}

	/** World-space union AABB for shadow “subject” geometry (casters / frustum driver), including fur shell margin; projector fallback if empty. */
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

	/** Directional shadow depth pass view-projection; optional receiver-aware XY/Z when using tight caster frustum. */
	static void SetupDirectionalShadowViewProjection(Light& MainLight, const math::AABB3& SubjectWorldAabb, bool bReceiverRelativeFrustumAdjust, const math::AABB3& ReceiverWorldAabb,
													 const core::vec2i& ShadowMapSize)
	{
		math::Vector3 wsSceneCorners[8];
		SubjectWorldAabb.GetPoint(wsSceneCorners);

		const math::Vector3 lightLookAt = SubjectWorldAabb.GetCenter();
		math::Vector3 lightUp = math::Vector3::UnitY;
		MainLight.Position = lightLookAt + (MainLight.Direction * LIGHT_DISTANCE);

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

		if (bReceiverRelativeFrustumAdjust)
		{
			math::AABB3 crop;
			math::AABB3 casterPad = math::ExpandAabbByMargin(SubjectWorldAabb, 4.f);
			if (ReceiverWorldAabb.GetIntersect(casterPad, crop))
				ExpandOrthoXYForWorldAabb(MainLight.LightView, crop, centerX, centerY, sizeX, sizeY);
			else
			{
				// Receiver outside padded caster AABB (large ground plane): still widen XY to receivers like glTFSample-style coverage.
				ExpandOrthoXYForWorldAabb(MainLight.LightView, ReceiverWorldAabb, centerX, centerY, sizeX, sizeY);
			}
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
		const int32_t SHADOW_WIDTH = 4096, SHADOW_HEIGHT = 4096;
		d->DepthRenderBuffer = d->RHI->RHICreateRenderTarget(RenderCore::EPixelFormat::PF_R32_FLOAT, SHADOW_WIDTH, SHADOW_HEIGHT, 1, false, true);
		if (!d->PointShadowCube)
			d->PointShadowCube = d->RHI->RHICreateTextureCube(RenderCore::EPixelFormat::PF_R32_FLOAT, kPointShadowCubeSize, kPointShadowCubeSize, 1, false);
	}

	void ShadowRenderPass::InvalidateCachedMainLightForShading()
	{
		C_P(ShadowRenderPass);
		d->bCachedMainLightValid = false;
		d->bCachedPointShadowValid = false;
	}

	bool ShadowRenderPass::TryGetCachedMainLightForShading(Light& OutLight)
	{
		C_P(ShadowRenderPass);
		if (!d->bCachedMainLightValid)
			return false;
		OutLight = d->CachedMainLightForShading;
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
		d->bCachedPointShadowValid = false;
		d->ShadowMgr->Update(Lights, ShadowProjectorScene);

		if (ShadowCasterMeshes.empty() && !ShadowProjectorScene.bValid)
			return;

		const std::vector<GltfSceneMeshInfo>* subjectMeshList = SelectShadowSubjectMeshListForFrustum(ShadowCasterMeshes, FrustumBoundsMeshes, ShadowProjectorScene);
		PruneStaleMeshShadowPS(d, ShadowCasterMeshes);

		const int mainDirIdx = FindFirstDirectionalLightIndex(Lights);
		const int pointShadowIdx = FindPointShadowCubeLightIndex(Lights);

		math::AABB3 subjectWorldAabb;
		bool subjectValid = false;
		BuildMergedShadowSubjectWorldAabb(subjectMeshList, ShadowProjectorScene, subjectWorldAabb, subjectValid);

		math::AABB3 receiverWorldAabb;
		bool receiverValid = false;
		BuildMergedShadowReceiverWorldAabb(FrustumBoundsMeshes, receiverWorldAabb, receiverValid);

		if (mainDirIdx >= 0 && subjectValid)
		{
			Light& mainLight = Lights[static_cast<size_t>(mainDirIdx)];
			mainLight.ShadowMapIndex = 0;
			const core::vec2i smSize = d->DepthRenderBuffer ? d->DepthRenderBuffer->GetSize() : core::vec2i{ 4096, 4096 };
			const bool bReceiverRelativeFrustumAdjust = receiverValid && kPreferTightShadowFrustumFromCasters && subjectMeshList == &ShadowCasterMeshes;
			SetupDirectionalShadowViewProjection(mainLight, subjectWorldAabb, bReceiverRelativeFrustumAdjust, receiverWorldAabb, smSize);
			d->CachedMainLightForShading = mainLight;
			d->bCachedMainLightValid = true;
			RenderDirectionalShadowMapPass(d, RHIContext, ShadowCasterMeshes, mainLight);
		}

		if (pointShadowIdx >= 0 && d->PointShadowCube && !ShadowCasterMeshes.empty())
			RenderPointLightShadowCubePass(d, RHIContext, ShadowCasterMeshes, Lights[static_cast<size_t>(pointShadowIdx)], pointShadowIdx);
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

} // namespace Engine
