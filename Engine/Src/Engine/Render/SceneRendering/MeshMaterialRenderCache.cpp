#include "Render/SceneRendering/MeshMaterialRenderCache.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "GltfModel/GltfMesh.h"
#include "Material/GltfMaterial.h"
#include "Material/FurMaterial.h"
#include "Render/PBRMaterialRender.h"
#include "Render/FurMaterialRender.h"
#include "Thread/RenderThread.h"

namespace Engine
{
	namespace
	{
		/** All cache mutations must match render recording worker (nested ENQUEUE runs inlined on that worker). */
		inline void VerifyMeshMaterialRenderCacheThread() noexcept
		{
#ifndef NDEBUG
			if (!GRenderThread)
			{
				assert(false && "FMeshMaterialRenderCache requires GRenderThread (render worker not started)");
				return;
			}
			assert(std::this_thread::get_id() == GRenderThread->GetWorkerThreadId()
				   && "FMeshMaterialRenderCache is render-worker-only — queue ENQUEUE_UNIQUE_RENDER_COMMAND or call from SceneRenderer path");
#else
			(void)0;
#endif
		}
	} // namespace

	void FMeshMaterialRenderCache::Clear() noexcept
	{
		VerifyMeshMaterialRenderCacheThread();
		CachedRenders.clear();
	}

	void FMeshMaterialRenderCache::InvalidateByStableSlotKey(uint64_t StableSlotKey) noexcept
	{
		VerifyMeshMaterialRenderCacheThread();
		if (StableSlotKey == 0)
			return;
		for (auto It = CachedRenders.begin(); It != CachedRenders.end();)
		{
			if (It->first.StableSlotKey == StableSlotKey)
				It = CachedRenders.erase(It);
			else
				++It;
		}
	}

	std::shared_ptr<MaterialRender> FMeshMaterialRenderCache::GetOrCreate(std::shared_ptr<MeshBase> Mesh, uint64_t StableMaterialRenderCacheKey)
	{
		VerifyMeshMaterialRenderCacheThread();
		FMaterialRenderCacheLookupKey Key{};
		Key.StableSlotKey = StableMaterialRenderCacheKey;
		Key.MeshBuffer = reinterpret_cast<uintptr_t>(Mesh->GetMeshBuffer().get());
		Key.Material = reinterpret_cast<uintptr_t>(Mesh->GetMaterial().get());
		Key.DeclaredVtxFeat = Mesh->GetMeshBuffer()->GetDeclaredVertexFeatures();

		{
			const auto Found = CachedRenders.find(Key);
			if (Found != CachedRenders.end())
				return Found->second;
		}

		std::shared_ptr<PBRMaterialRender> PBRMaterial;
		switch (Mesh->GetMaterial()->GetMaterialType())
		{
		case MaterialBase::MaterialType::PBR:
			PBRMaterial = std::make_shared<PBRMaterialRender>(Mesh->GetMeshBuffer(), Mesh->GetMaterial());
			break;
		case MaterialBase::MaterialType::FUR:
		{
			auto FurMat = std::static_pointer_cast<Engine::FurMaterial>(Mesh->GetMaterial());
			PBRMaterial = std::make_shared<FurMaterialRender>(Mesh->GetMeshBuffer(), FurMat, FurMat->GetFurConfig(), FurMat->GetNoiseTex());
		}
		break;
		default:
			PBRMaterial = std::make_shared<PBRMaterialRender>(Mesh->GetMeshBuffer(), Mesh->GetMaterial());
			break;
		}

		PBRMaterial->InitRenderResource();
		// InitShader uses ENQUEUE_UNIQUE_RENDER_COMMAND on this worker; AppendCommand runs it inline — no queue drain needed.
		// FlushRenderingCommands here used to PumpRecordingQueueUntilEmpty and could nest a second ExecuteFrame mid-pass,
		// breaking ImGui (double NewFrame) and frame boundaries.

		// Same render worker thread may have inserted this key via nested initialization / second GetOrCreate.
		const auto FoundAgain = CachedRenders.find(Key);
		if (FoundAgain != CachedRenders.end())
			return FoundAgain->second;
		CachedRenders.emplace(Key, PBRMaterial);
		return PBRMaterial;
	}
} // namespace Engine
