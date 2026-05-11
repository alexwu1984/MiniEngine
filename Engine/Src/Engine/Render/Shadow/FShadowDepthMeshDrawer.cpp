#include "Render/Shadow/FShadowDepthMeshDrawer.h"
#include "Render/Shadow/FShadowSceneBounds.h"
#include "Render/Shadow/ShadowPS.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIRenderTarget.h"
#include "RHI/RHITextureCube.h"
#include "GltfModel/GltfMesh.h"
#include "Scene/SceneMeshComponent.h"
#include <unordered_set>

namespace Engine
{
	FShadowDepthMeshDrawer::FShadowDepthMeshDrawer(RenderCore::DynamicRHI* InRHI)
		: RHI(InRHI)
	{
	}

	void FShadowDepthMeshDrawer::UpdateShadowPSPaletteForMesh(const std::shared_ptr<ShadowPS>& shadowRender, const std::shared_ptr<MeshBase>& Mesh)
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

	void FShadowDepthMeshDrawer::PruneStaleMeshShadowPasses(const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes,
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
		for (auto it = ShadowRenders.begin(); it != ShadowRenders.end();)
		{
			if (!it->first || casterMeshPtrs.find(it->first.get()) == casterMeshPtrs.end())
				it = ShadowRenders.erase(it);
			else
				++it;
		}
	}

	void FShadowDepthMeshDrawer::ClearCache()
	{
		ShadowRenders.clear();
	}

	void FShadowDepthMeshDrawer::DrawDirectional(RenderCore::RHICommandContext& RHIContext, const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const Light& LightForShadow,
												 const std::shared_ptr<RenderCore::RHIRenderTarget>& Target)
	{
		for (const auto& MeshInfo : ShadowCasterMeshes)
		{
			for (size_t MeshIndex = 0; MeshIndex < MeshInfo.Meshes.size(); ++MeshIndex)
			{
				std::shared_ptr<MeshBase> Mesh = MeshInfo.Meshes[MeshIndex];
				if (!Mesh || !FShadowSceneBounds::MeshWritesShadowMapDepth(Mesh))
					continue;
				auto& shadowRender = ShadowRenders[Mesh];
				if (!shadowRender)
				{
					shadowRender = std::make_shared<ShadowPS>(RHI, Mesh);
					shadowRender->InitResource();
				}
				UpdateShadowPSPaletteForMesh(shadowRender, Mesh);
				shadowRender->Draw(RHIContext, Mesh->GetMeshMat() * MeshInfo.WorldTransform, LightForShadow, Target);
			}
		}
	}

	void FShadowDepthMeshDrawer::DrawCubeFace(RenderCore::RHICommandContext& RHIContext, const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes, const Light& FaceLight,
											  const std::shared_ptr<RenderCore::RHITextureCube>& Cube, int FaceIndex)
	{
		for (const auto& MeshInfo : ShadowCasterMeshes)
		{
			for (size_t MeshIndex = 0; MeshIndex < MeshInfo.Meshes.size(); ++MeshIndex)
			{
				std::shared_ptr<MeshBase> Mesh = MeshInfo.Meshes[MeshIndex];
				if (!Mesh || !FShadowSceneBounds::MeshWritesShadowMapDepth(Mesh))
					continue;
				auto& shadowRender = ShadowRenders[Mesh];
				if (!shadowRender)
				{
					shadowRender = std::make_shared<ShadowPS>(RHI, Mesh);
					shadowRender->InitResource();
				}
				UpdateShadowPSPaletteForMesh(shadowRender, Mesh);
				shadowRender->DrawCubeFace(RHIContext, Mesh->GetMeshMat() * MeshInfo.WorldTransform, FaceLight, Cube, FaceIndex);
			}
		}
	}
}
