#include "Render/SceneRendering/OpaqueMeshDrawBuilder.h"
#include "Render/SceneRendering/DeferredBasePassMeshDispatch.h"
#include "Render/SceneRendering/MeshMaterialRenderCache.h"
#include "Render/SceneRendering/TranslucentMeshSorter.h"
#include "Render/SceneRendering/TranslucentMeshSortKey.h"
#include "Render/SceneRendering/SceneMaterialShaderParameters.h"
#include "Engine/Render/PBRMaterialRender.h"
#include "Engine/Render/FurMaterialRender.h"
#include "RHI/DynamicRHI.h"
#include "Scene/SceneMeshComponent.h"
#include "GltfModel/GltfMesh.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "Material/MaterialBase.h"

namespace Engine
{
	namespace
	{
		bool MeshesCompatibleForPBROpaqueBatch(MeshBase* A, MeshBase* B)
		{
			if (!A || !B)
				return false;
			if (A->HasSkin() || B->HasSkin())
				return false;
			const auto bufA = A->GetMeshBuffer().get();
			const auto bufB = B->GetMeshBuffer().get();
			if (!bufA || bufA != bufB)
				return false;
			const MaterialBase* ma = A->GetMaterial().get();
			const MaterialBase* mb = B->GetMaterial().get();
			if (!ma || !mb)
				return false;
			if (ma->GetStableMaterialInstanceId() != mb->GetStableMaterialInstanceId())
				return false;
			return std::memcmp(A->GetMeshMat().m, B->GetMeshMat().m, sizeof(math::Matrix4x4)) == 0;
		}

		bool BatchSortOpaqueLess(const FTranslucentMeshSortKey& A, const FTranslucentMeshSortKey& B)
		{
			MeshBase* Ma = A.Mesh.get();
			MeshBase* Mb = B.Mesh.get();
			if (!Ma || !Mb)
				return Ma < Mb;
			const std::shared_ptr<MaterialBase> matA = Ma->GetMaterial();
			const std::shared_ptr<MaterialBase> matB = Mb->GetMaterial();
			if (!matA || !matB)
				return Ma < Mb;
			const uint64_t ia = matA->GetStableMaterialInstanceId();
			const uint64_t ib = matB->GetStableMaterialInstanceId();
			if (ia != ib)
				return ia < ib;
			const uintptr_t bufa = reinterpret_cast<uintptr_t>(Ma->GetMeshBuffer().get());
			const uintptr_t bufb = reinterpret_cast<uintptr_t>(Mb->GetMeshBuffer().get());
			if (bufa != bufb)
				return bufa < bufb;
			const uint32_t va = Ma->GetMeshBuffer() ? Ma->GetMeshBuffer()->GetDeclaredVertexFeatures() : 0u;
			const uint32_t vb = Mb->GetMeshBuffer() ? Mb->GetMeshBuffer()->GetDeclaredVertexFeatures() : 0u;
			if (va != vb)
				return va < vb;
			const int mc = std::memcmp(Ma->GetMeshMat().m, Mb->GetMeshMat().m, sizeof(math::Matrix4x4));
			if (mc != 0)
				return mc < 0;
			return Ma < Mb;
		}
	} // namespace

	void FOpaqueMeshDrawBuilder::DrawSortedOpaqueMeshes(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos, const math::Vector3& CameraWorldPos,
													  bool bIsPrePass, const FDeferredBasePassDrawContext& DrawContext, FMeshMaterialRenderCache& MaterialCache)
	{
		std::vector<FTranslucentMeshSortKey> Flat;
		for (const auto& SceneMeshInfo : SceneMeshInfos)
			FTranslucentMeshSorter::AppendPerActorMeshSortKeys(SceneMeshInfo, CameraWorldPos, Flat);

		FTranslucentMeshSorter::SortByDistance(Flat);
		std::stable_sort(Flat.begin(), Flat.end(), [](const FTranslucentMeshSortKey& A, const FTranslucentMeshSortKey& B) {
			const uint64_t Sa =
				(A.Mesh && A.Mesh->GetMaterial()) ? A.Mesh->GetMaterial()->GetStableMaterialInstanceId() : 0ull;
			const uint64_t Sb =
				(B.Mesh && B.Mesh->GetMaterial()) ? B.Mesh->GetMaterial()->GetStableMaterialInstanceId() : 0ull;
			return Sa < Sb;
		});
		std::stable_sort(Flat.begin(), Flat.end(), BatchSortOpaqueLess);

		size_t i = 0;
		while (i < Flat.size())
		{
			const FTranslucentMeshSortKey& Key = Flat[i];
			const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
			std::shared_ptr<MaterialRender> Mat = MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey);
			const std::shared_ptr<MaterialBase> meshMat = Mesh ? Mesh->GetMaterial() : nullptr;
			if (!Mesh || !meshMat || meshMat->IsTransparent())
			{
				++i;
				continue;
			}

			MeshBase* LeadMesh = Mesh.get();
			auto* strictPBR = (Mat.get() && typeid(*Mat.get()) == typeid(PBRMaterialRender)) ? static_cast<PBRMaterialRender*>(Mat.get()) : nullptr;
			auto* furMat = dynamic_cast<FurMaterialRender*>(Mat.get());

			size_t end = i + 1;
			if (strictPBR && LeadMesh && !LeadMesh->HasSkin() && !furMat)
			{
				while (end < Flat.size())
				{
					const FTranslucentMeshSortKey& K2 = Flat[end];
					std::shared_ptr<MeshBase> M2 = K2.Mesh;
					const auto mat2 = M2 ? M2->GetMaterial() : nullptr;
					if (!M2 || !mat2 || mat2->IsTransparent())
						break;
					std::shared_ptr<MaterialRender> Mat2 = MaterialCache.GetOrCreate(M2, K2.MaterialRenderCacheKey);
					if (!Mat2.get() || typeid(*Mat2.get()) != typeid(PBRMaterialRender))
						break;
					if (dynamic_cast<FurMaterialRender*>(Mat2.get()))
						break;
					if (!MeshesCompatibleForPBROpaqueBatch(LeadMesh, M2.get()))
						break;
					++end;
				}
			}

			if (strictPBR && end > i + 1 && LeadMesh && !LeadMesh->HasSkin() && !furMat)
			{
				MaterialRenderParam P0 = FSceneMaterialShaderParameters::BuildForDeferredBasePass(
					DrawContext.WorldSceneRender, DrawContext.ViewData ? DrawContext.ViewData.get() : nullptr, LeadMesh, Key.WorldTransform, Key.PrevWorldTransform,
					DrawContext.TargetBuffer);
				strictPBR->BeginDeferredOpaqueDrawBatch(*DrawContext.RHICmdList, P0);
				for (size_t j = i + 1; j < end; ++j)
				{
					const FTranslucentMeshSortKey& Kj = Flat[j];
					MeshBase* Mj = Kj.Mesh.get();
					MaterialRenderParam Pj = FSceneMaterialShaderParameters::BuildForDeferredBasePass(
						DrawContext.WorldSceneRender, DrawContext.ViewData ? DrawContext.ViewData.get() : nullptr, Mj, Kj.WorldTransform, Kj.PrevWorldTransform,
						DrawContext.TargetBuffer);
					strictPBR->DrawDeferredOpaqueBatchInstance(*DrawContext.RHICmdList, Pj);
				}
				i = end;
				continue;
			}

			FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform, Mat, bIsPrePass, DrawContext);
			++i;
		}
	}
}
