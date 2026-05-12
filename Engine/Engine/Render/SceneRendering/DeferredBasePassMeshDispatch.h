#pragma once
#include "Render/MaterialRender.h"
#include "Render/SceneRendering/DeferredBasePassDrawContext.h"
#include <memory>

namespace Engine
{
	class MeshBase;

	/** Submits a single mesh material draw for the deferred base pass (velocity pre-pass or main pass). */
	class FDeferredBasePassMeshDispatch
	{
	public:
		static void Dispatch(const std::shared_ptr<MeshBase>& Mesh, const math::Matrix4x4& WorldTransform, const math::Matrix4x4& PrevWorldTransform,
							 const std::shared_ptr<MaterialRender>& Material, bool bIsPrePass, const FDeferredBasePassDrawContext& DrawContext);
	};
}
